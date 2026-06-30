/*
 * audio_host_probe.cpp — Windows WASAPI host-boundary probes for the perpetual
 * audio-crackle investigation (see F:\Projects\_audio_round2\AUDIO.md).
 *
 * The recompiled RSP audio is proven bit-exact vs ares, and the runner's PCM is
 * clean up to the SDL callback (tap T3). The artifact lives DOWNSTREAM of T3, in
 * the SDL -> WASAPI shared-mode -> Windows audio engine -> endpoint path, which our
 * existing taps cannot see. This module adds two host-side probes:
 *
 *   1. psr_log_wasapi_mix_format(): query the TRUE engine mix format via
 *      IAudioClient::GetMixFormat (+ GetDevicePeriod). SDL's reported `obtained`
 *      rate can hide a shared-mode resample; this reports what Windows actually
 *      mixes at, so we can target it exactly (one resampler, not two).
 *
 *   2. psr_audio_t5_start(): an always-on "T5" tap that captures, via WASAPI
 *      LOOPBACK, exactly the PCM Windows sends the render endpoint — the first
 *      measurement that crosses the T3 wall. This is a DIGITAL capture of the OS
 *      render mix (IAudioClient AUDCLNT_STREAMFLAGS_LOOPBACK), NOT a microphone /
 *      acoustic capture, so it respects the no-acoustic-capture constraint. Gated
 *      by PSR_AUDIO_T5_LOOPBACK=1; the captured frames feed the existing
 *      recomp_audio_debug "T5" tap, so the F9 ring dump produces T5.wav alongside
 *      T1/T2/T3. Compare T3 vs T5:
 *        T3 clean & T5 dirty -> OS/WASAPI/APO path introduced it (likely our
 *                               device-open config -> fixable in the runner).
 *        T3 clean & T5 clean -> endpoint/driver/hardware (external).
 */

#include <cstdio>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

// From recomp_audio_debug.h (single-header tap ring; linked via main.cpp).
extern "C" void recomp_audio_debug_push_f32(const char* tap, const float* interleaved,
                                            int frames, double rate, int channels);
extern "C" int  recomp_audio_debug_enabled(void);

namespace {

const CLSID kCLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID   kIID_IMMDeviceEnumerator  = __uuidof(IMMDeviceEnumerator);
const IID   kIID_IAudioClient          = __uuidof(IAudioClient);
const IID   kIID_IAudioCaptureClient   = __uuidof(IAudioCaptureClient);
const IID   kIID_IAudioRenderClient    = __uuidof(IAudioRenderClient);

std::atomic<bool> g_t5_run{false};
std::thread       g_t5_thread;

// Resolve a WAVEFORMATEX to (is_float, valid_pcm16, sample_rate, channels).
struct FmtInfo { bool is_float; bool is_pcm16; unsigned rate; unsigned ch; unsigned bits; };
FmtInfo classify(const WAVEFORMATEX* wf) {
    FmtInfo fi{false, false, wf->nSamplesPerSec, wf->nChannels, wf->wBitsPerSample};
    WORD tag = wf->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        // Resolve the EXTENSIBLE subformat by GUID Data1 (avoids the ksmedia.h
        // KSDATAFORMAT_SUBTYPE_* GUID symbols / extra link lib): IEEE_FLOAT=3, PCM=1.
        const WAVEFORMATEXTENSIBLE* we = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
        if      (we->SubFormat.Data1 == 0x00000003) tag = WAVE_FORMAT_IEEE_FLOAT;
        else if (we->SubFormat.Data1 == 0x00000001) tag = WAVE_FORMAT_PCM;
    }
    if (tag == WAVE_FORMAT_IEEE_FLOAT && wf->wBitsPerSample == 32) fi.is_float = true;
    if (tag == WAVE_FORMAT_PCM && wf->wBitsPerSample == 16)        fi.is_pcm16 = true;
    return fi;
}

void t5_thread_fn() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr);  // S_FALSE = already inited on this thread

    IMMDeviceEnumerator* en  = nullptr;
    IMMDevice*           dev = nullptr;
    IAudioClient*        ac  = nullptr;
    IAudioCaptureClient* cap = nullptr;
    WAVEFORMATEX*        mix = nullptr;

    auto cleanup = [&]() {
        if (cap) cap->Release();
        if (mix) CoTaskMemFree(mix);
        if (ac)  ac->Release();
        if (dev) dev->Release();
        if (en)  en->Release();
        if (com_inited) CoUninitialize();
    };

    if (FAILED(CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                kIID_IMMDeviceEnumerator, (void**)&en))) { cleanup(); return; }
    if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)))    { cleanup(); return; }
    if (FAILED(dev->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&ac))) { cleanup(); return; }
    if (FAILED(ac->GetMixFormat(&mix)) || !mix)                          { cleanup(); return; }

    FmtInfo fi = classify(mix);
    if (!fi.is_float && !fi.is_pcm16) {
        std::fprintf(stderr, "[PSR][audio][T5] unsupported mix format tag=0x%04x bits=%u — T5 disabled\n",
                     mix->wFormatTag, mix->wBitsPerSample);
        cleanup(); return;
    }

    // Loopback shared-mode capture of the render endpoint's engine mix. 200 ms ring.
    const REFERENCE_TIME kDur = 2000000; // 200 ms in 100-ns units
    if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                              kDur, 0, mix, nullptr))) { cleanup(); return; }
    if (FAILED(ac->GetService(kIID_IAudioCaptureClient, (void**)&cap))) { cleanup(); return; }
    if (FAILED(ac->Start())) { cleanup(); return; }

    std::fprintf(stderr, "[PSR][audio][T5] loopback capture started (%u Hz, %u ch, %s)\n",
                 fi.rate, fi.ch, fi.is_float ? "f32" : "s16");

    std::vector<float> scratch;
    while (g_t5_run.load(std::memory_order_relaxed)) {
        UINT32 packet = 0;
        if (FAILED(cap->GetNextPacketSize(&packet))) break;
        if (packet == 0) { Sleep(5); continue; }

        BYTE*  data = nullptr;
        UINT32 frames = 0;
        DWORD  flags = 0;
        if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
        if (frames > 0) {
            scratch.resize((size_t)frames * fi.ch);
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::fill(scratch.begin(), scratch.end(), 0.0f);
            } else if (fi.is_float) {
                std::copy((const float*)data, (const float*)data + scratch.size(), scratch.begin());
            } else { // s16 -> f32
                const int16_t* s = (const int16_t*)data;
                for (size_t i = 0; i < scratch.size(); i++) scratch[i] = s[i] * (1.0f / 32768.0f);
            }
            recomp_audio_debug_push_f32("T5", scratch.data(), (int)frames, (double)fi.rate, (int)fi.ch);
        }
        cap->ReleaseBuffer(frames);
    }

    ac->Stop();
    cleanup();
}

} // namespace

extern "C" void psr_log_wasapi_mix_format(void) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr);
    IMMDeviceEnumerator* en = nullptr;
    IMMDevice* dev = nullptr;
    IAudioClient* ac = nullptr;
    WAVEFORMATEX* mix = nullptr;
    if (SUCCEEDED(CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                   kIID_IMMDeviceEnumerator, (void**)&en)) &&
        SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&ac)) &&
        SUCCEEDED(ac->GetMixFormat(&mix)) && mix) {
        FmtInfo fi = classify(mix);
        REFERENCE_TIME def_period = 0, min_period = 0;
        ac->GetDevicePeriod(&def_period, &min_period);
        std::fprintf(stderr,
            "[PSR][audio] WASAPI engine mix: %u Hz, %u ch, %u-bit %s (tag=0x%04x)  "
            "devicePeriod=%.2fms min=%.2fms\n",
            fi.rate, fi.ch, fi.bits, fi.is_float ? "float" : (fi.is_pcm16 ? "pcm" : "?"),
            mix->wFormatTag, def_period / 10000.0, min_period / 10000.0);
    } else {
        std::fprintf(stderr, "[PSR][audio] WASAPI GetMixFormat probe unavailable (non-WASAPI or COM failure)\n");
    }
    if (mix) CoTaskMemFree(mix);
    if (ac)  ac->Release();
    if (dev) dev->Release();
    if (en)  en->Release();
    if (com_inited) CoUninitialize();
}

extern "C" void psr_audio_t5_start(void) {
    if (g_t5_run.load()) return;                       // already running
    const char* e = getenv("PSR_AUDIO_T5_LOOPBACK");
    if (!e || !*e || e[0] == '0') return;              // opt-in only
    if (!recomp_audio_debug_enabled()) {
        std::fprintf(stderr, "[PSR][audio][T5] PSR_AUDIO_T5_LOOPBACK set but RECOMP_AUDIO_DEBUG "
                             "is off — T5 frames would have nowhere to land; not starting.\n");
        return;
    }
    g_t5_run.store(true);
    g_t5_thread = std::thread(t5_thread_fn);
}

extern "C" void psr_audio_t5_stop(void) {
    if (!g_t5_run.exchange(false)) return;
    if (g_t5_thread.joinable()) g_t5_thread.join();
}

// ===========================================================================
// Native WASAPI render path (cpal-equivalent) — opt-in alternative to SDL2 audio.
//
// Rationale: generation is bit-exact and tap T3 is bit-clean, so the crackle is in
// the SDL2 -> WASAPI delivery layer. JRickey's (known-good) gba-recomp renders via
// cpal: shared-mode, EVENT-DRIVEN, the device's native mix format, the engine's
// DEFAULT period buffer, on an MMCSS "Pro Audio" thread. We replicate that exactly
// here and feed it from the SAME bridge (psr_audio_fill_f32), so it is byte-identical
// to the SDL path EXCEPT for how/when frames are delivered to the OS. Gated by
// PSR_AUDIO_WASAPI=1 (when set, main.cpp skips SDL_OpenAudioDevice for output).
// ===========================================================================

extern "C" void psr_audio_fill_f32(float* out, int frames);  // from main.cpp

namespace {
std::atomic<bool> g_render_run{false};
std::thread       g_render_thread;
unsigned          g_render_rate = 0;   // engine mix rate, for the bridge

void render_thread_fn() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr);

    IMMDeviceEnumerator* en  = nullptr;
    IMMDevice*           dev = nullptr;
    IAudioClient*        ac  = nullptr;
    IAudioRenderClient*  rc  = nullptr;
    WAVEFORMATEX*        mix = nullptr;
    HANDLE               evt = nullptr;
    HANDLE               mmcss = nullptr;
    DWORD                mmcss_idx = 0;

    auto cleanup = [&]() {
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (rc)  rc->Release();
        if (evt) CloseHandle(evt);
        if (mix) CoTaskMemFree(mix);
        if (ac)  ac->Release();
        if (dev) dev->Release();
        if (en)  en->Release();
        if (com_inited) CoUninitialize();
    };

    if (FAILED(CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                kIID_IMMDeviceEnumerator, (void**)&en)) ||
        FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) ||
        FAILED(dev->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&ac)) ||
        FAILED(ac->GetMixFormat(&mix)) || !mix) {
        std::fprintf(stderr, "[PSR][audio][wasapi] device/format setup failed — WASAPI render off\n");
        cleanup(); return;
    }
    FmtInfo fi = classify(mix);
    if (!fi.is_float || fi.ch != 2) {
        std::fprintf(stderr, "[PSR][audio][wasapi] mix format not f32 stereo (tag=0x%04x %u-bit %uch) — WASAPI render off\n",
                     mix->wFormatTag, fi.bits, fi.ch);
        cleanup(); return;
    }
    g_render_rate = fi.rate;

    // Shared-mode, event-driven, engine DEFAULT period (hnsBufferDuration=0).
    if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                              0, 0, mix, nullptr))) { cleanup(); return; }
    evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!evt || FAILED(ac->SetEventHandle(evt))) { cleanup(); return; }
    if (FAILED(ac->GetService(kIID_IAudioRenderClient, (void**)&rc))) { cleanup(); return; }

    UINT32 buf_frames = 0;
    if (FAILED(ac->GetBufferSize(&buf_frames)) || buf_frames == 0) { cleanup(); return; }

    // Prime the full buffer with one fill before Start (avoids an initial glitch).
    BYTE* data = nullptr;
    if (SUCCEEDED(rc->GetBuffer(buf_frames, &data))) {
        psr_audio_fill_f32(reinterpret_cast<float*>(data), (int)buf_frames);
        rc->ReleaseBuffer(buf_frames, 0);
    }

    // Pro-audio MMCSS priority — the likely delta vs SDL's audio thread: a render
    // thread that never misses the engine period under RT64/CPU load.
    mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcss_idx);

    REFERENCE_TIME def_period = 0, min_period = 0;
    ac->GetDevicePeriod(&def_period, &min_period);
    if (FAILED(ac->Start())) { cleanup(); return; }
    std::fprintf(stderr, "[PSR][audio][wasapi] render started: %u Hz f32 2ch, bufFrames=%u (%.2fms), "
                 "period=%.2fms, ProAudio=%s\n",
                 fi.rate, buf_frames, buf_frames * 1000.0 / fi.rate, def_period / 10000.0,
                 mmcss ? "yes" : "no");

    while (g_render_run.load(std::memory_order_relaxed)) {
        DWORD w = WaitForSingleObject(evt, 2000);
        if (w != WAIT_OBJECT_0) continue;
        UINT32 pad = 0;
        if (FAILED(ac->GetCurrentPadding(&pad))) break;
        UINT32 avail = (pad <= buf_frames) ? (buf_frames - pad) : 0;
        if (avail == 0) continue;
        if (FAILED(rc->GetBuffer(avail, &data))) break;
        psr_audio_fill_f32(reinterpret_cast<float*>(data), (int)avail);
        rc->ReleaseBuffer(avail, 0);
    }

    ac->Stop();
    cleanup();
}
} // namespace

// Returns the WASAPI engine mix rate (Hz) so the bridge can target it, or 0 on
// failure / non-WASAPI. (Cheap one-shot query; mirrors psr_log_wasapi_mix_format.)
extern "C" unsigned psr_query_wasapi_mix_rate(void) {
    unsigned rate = 0;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool ci = SUCCEEDED(hr);
    IMMDeviceEnumerator* en = nullptr; IMMDevice* dev = nullptr;
    IAudioClient* ac = nullptr; WAVEFORMATEX* mix = nullptr;
    if (SUCCEEDED(CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                   kIID_IMMDeviceEnumerator, (void**)&en)) &&
        SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&ac)) &&
        SUCCEEDED(ac->GetMixFormat(&mix)) && mix) {
        rate = mix->nSamplesPerSec;
    }
    if (mix) CoTaskMemFree(mix);
    if (ac) ac->Release(); if (dev) dev->Release(); if (en) en->Release();
    if (ci) CoUninitialize();
    return rate;
}

// Returns 1 and starts the native WASAPI render path if PSR_AUDIO_WASAPI=1.
extern "C" int psr_audio_wasapi_enabled(void) {
    const char* e = getenv("PSR_AUDIO_WASAPI");
    return (e && *e && e[0] != '0') ? 1 : 0;
}

extern "C" int psr_audio_wasapi_start(void) {
    if (g_render_run.load()) return 1;
    if (!psr_audio_wasapi_enabled()) return 0;
    g_render_run.store(true);
    g_render_thread = std::thread(render_thread_fn);
    return 1;
}

extern "C" void psr_audio_wasapi_stop(void) {
    if (!g_render_run.exchange(false)) return;
    if (g_render_thread.joinable()) g_render_thread.join();
}

#else  // non-Windows: no-op stubs

extern "C" void psr_log_wasapi_mix_format(void) {}
extern "C" void psr_audio_t5_start(void) {}
extern "C" void psr_audio_t5_stop(void) {}
extern "C" unsigned psr_query_wasapi_mix_rate(void) { return 0; }
extern "C" int  psr_audio_wasapi_enabled(void) { return 0; }
extern "C" int  psr_audio_wasapi_start(void) { return 0; }
extern "C" void psr_audio_wasapi_stop(void) {}

#endif
