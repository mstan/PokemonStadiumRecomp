/*
 * rt64_render_context.cpp — Pokemon Stadium's RT64 wrapper.
 *
 * Adapted from Zelda64Recomp's src/main/rt64_render_context.cpp
 * (commit ab677e7). Same RT64 boilerplate; texture pack subsystem
 * removed (depends on mod loader we don't ship), overloaded.h
 * dependency dropped.
 *
 * The recompui render hook was restored 2026-06 for the SS Anne UI:
 * pkmnstadium::ui_seam::install() (src/main/ui_seam.cpp) registers an
 * RmlUi overlay via RT64::SetRenderHooks before app->setup(). Phase 0
 * draws a trivial document to prove the RmlUi<->RT64 seam; Phase 1
 * replaces it with the launcher.
 *
 * Modified 2026 by Matthew Stanley:
 *   - Diagnostic env-var overrides for menu-sprite-corruption
 *     investigation (PSR_RT64_RES_MULT, PSR_RT64_FILTERING,
 *     PSR_RT64_UPSCALE2D, PSR_RT64_THREEPOINT) so we can short-
 *     circuit user-config to specific values without touching
 *     persistent state.
 *
 * Copyright (c) 2026 Matthew Stanley
 */

// NOMINMAX must be defined before any include that pulls in Windows.h
// (RT64 headers do, transitively). Without it, Windows.h defines `max`
// and `min` as macros, breaking std::max/std::min calls.
#define NOMINMAX

#include <memory>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cctype>
#include <string>

#define HLSL_CPU
#include "hle/rt64_application.h"

#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#include "librecomp/rdp.hpp"

#include "pokestadium_render.h"
#include "debug_server.h"
#include "ui_seam.h"

static RT64::UserConfiguration::Antialiasing device_max_msaa = RT64::UserConfiguration::Antialiasing::None;
static bool sample_positions_supported = false;
static bool high_precision_fb_enabled = false;

static uint8_t DMEM[0x1000];
static uint8_t IMEM[0x1000];

unsigned int MI_INTR_REG = 0;

static void dummy_check_interrupts() {}

static RT64::UserConfiguration::Antialiasing compute_max_supported_aa(RT64::RenderSampleCounts bits) {
    if (bits & RT64::RenderSampleCount::Bits::COUNT_2) {
        if (bits & RT64::RenderSampleCount::Bits::COUNT_4) {
            if (bits & RT64::RenderSampleCount::Bits::COUNT_8) {
                return RT64::UserConfiguration::Antialiasing::MSAA8X;
            }
            return RT64::UserConfiguration::Antialiasing::MSAA4X;
        }
        return RT64::UserConfiguration::Antialiasing::MSAA2X;
    }
    return RT64::UserConfiguration::Antialiasing::None;
}

static RT64::UserConfiguration::AspectRatio to_rt64(ultramodern::renderer::AspectRatio option) {
    switch (option) {
        case ultramodern::renderer::AspectRatio::Original:    return RT64::UserConfiguration::AspectRatio::Original;
        case ultramodern::renderer::AspectRatio::Expand:      return RT64::UserConfiguration::AspectRatio::Expand;
        case ultramodern::renderer::AspectRatio::Manual:      return RT64::UserConfiguration::AspectRatio::Manual;
        case ultramodern::renderer::AspectRatio::OptionCount: return RT64::UserConfiguration::AspectRatio::OptionCount;
    }
    return RT64::UserConfiguration::AspectRatio::Original;
}

static RT64::UserConfiguration::Antialiasing to_rt64(ultramodern::renderer::Antialiasing option) {
    switch (option) {
        case ultramodern::renderer::Antialiasing::None:        return RT64::UserConfiguration::Antialiasing::None;
        case ultramodern::renderer::Antialiasing::MSAA2X:      return RT64::UserConfiguration::Antialiasing::MSAA2X;
        case ultramodern::renderer::Antialiasing::MSAA4X:      return RT64::UserConfiguration::Antialiasing::MSAA4X;
        case ultramodern::renderer::Antialiasing::MSAA8X:      return RT64::UserConfiguration::Antialiasing::MSAA8X;
        case ultramodern::renderer::Antialiasing::OptionCount: return RT64::UserConfiguration::Antialiasing::OptionCount;
    }
    return RT64::UserConfiguration::Antialiasing::None;
}

static RT64::UserConfiguration::RefreshRate to_rt64(ultramodern::renderer::RefreshRate option) {
    switch (option) {
        case ultramodern::renderer::RefreshRate::Original:    return RT64::UserConfiguration::RefreshRate::Original;
        case ultramodern::renderer::RefreshRate::Display:     return RT64::UserConfiguration::RefreshRate::Display;
        case ultramodern::renderer::RefreshRate::Manual:      return RT64::UserConfiguration::RefreshRate::Manual;
        case ultramodern::renderer::RefreshRate::OptionCount: return RT64::UserConfiguration::RefreshRate::OptionCount;
    }
    return RT64::UserConfiguration::RefreshRate::Original;
}

static RT64::UserConfiguration::InternalColorFormat to_rt64(ultramodern::renderer::HighPrecisionFramebuffer option) {
    switch (option) {
        case ultramodern::renderer::HighPrecisionFramebuffer::Off:         return RT64::UserConfiguration::InternalColorFormat::Standard;
        case ultramodern::renderer::HighPrecisionFramebuffer::On:          return RT64::UserConfiguration::InternalColorFormat::High;
        case ultramodern::renderer::HighPrecisionFramebuffer::Auto:        return RT64::UserConfiguration::InternalColorFormat::Automatic;
        case ultramodern::renderer::HighPrecisionFramebuffer::OptionCount: return RT64::UserConfiguration::InternalColorFormat::OptionCount;
    }
    return RT64::UserConfiguration::InternalColorFormat::Automatic;
}

static void set_application_user_config(RT64::Application* application,
                                        const ultramodern::renderer::GraphicsConfig& config) {
    // SUPERSAMPLING — Manual mode, driven by the launcher's menu-safe preset
    // (Settings > Supersampling: off|2x|4x, default 2x). The N64 models look
    // awful without SSAA (rainbow speckle on sub-pixel-thin geometry), so we do
    // NOT let config.res_option decide it. The preset keeps the PRESENTED image
    // at resMult/downsample == 3 (960x720, the window's native 3x scale) — the
    // ratio the #12 uniform framebuffer-scale fix keeps the 2D menus correct
    // under — and only varies the supersampling depth. Default 2x == render
    // 1920x1440, box-filter down by 2 to 720p (the historical hardcoded value).
    application->userConfig.resolution = RT64::UserConfiguration::Resolution::Manual;
    {
        // ds_option carries the launcher's supersampling preset (1/2/4). Render
        // at 3*ds the N64 base and box-filter down by ds, so the PRESENTED image
        // stays at 3x (the native window scale the #12 menu fix needs). Guard an
        // unset/invalid value to the historical 2x default.
        int ds = config.ds_option;
        if (ds != 1 && ds != 2 && ds != 4) ds = 2;
        application->userConfig.resolutionMultiplier = 3.0 * static_cast<double>(ds);
        application->userConfig.downsampleMultiplier = ds;
    }

    switch (config.hr_option) {
        default:
        case ultramodern::renderer::HUDRatioMode::Original:
            application->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Original;
            break;
        case ultramodern::renderer::HUDRatioMode::Clamp16x9:
            application->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Manual;
            application->userConfig.extAspectTarget = 16.0/9.0;
            break;
        case ultramodern::renderer::HUDRatioMode::Full:
            application->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Expand;
            break;
    }

    application->userConfig.aspectRatio = to_rt64(config.ar_option);
    // Anti-aliasing — driven by the launcher's msaa_option (Settings >
    // Antialiasing: off|2x|4x|8x, default 4x). The N64 models' hard polygon
    // silhouettes alias badly without it, so the default stays 4x. (PSR_RT64_MSAA
    // below still overrides raw for A/B testing.)
    application->userConfig.antialiasing = to_rt64(config.msaa_option);
    application->userConfig.refreshRate = to_rt64(config.rr_option);
    application->userConfig.refreshRateTarget = config.rr_manual_value;
    application->userConfig.internalColorFormat = to_rt64(config.hpfb_option);
    application->userConfig.displayBuffering = RT64::UserConfiguration::DisplayBuffering::Triple;

    // Three-point filtering produces horizontal color leaks on Stadium's
    // clamped 2D menu panel textures. Keep the override below available for
    // renderer A/B tests, but default this port to true bilinear sampling.
    application->userConfig.threePointFiltering = false;

    // PSR diagnostic overrides for menu-sprite-corruption investigation
    // (dev/sprite-corruption-menu-borders branch). RT64's defaults are
    // filtering=AntiAliasedPixelScaling, upscale2D=ScaledOnly,
    // threePointFiltering=true. Setting env vars short-circuits to specific values; an
    // unset/empty env var leaves the default.
    if (const char* r = std::getenv("PSR_RT64_RES_MULT")) {
        char* end = nullptr;
        double m = std::strtod(r, &end);
        if (end != r && m >= 1.0 && m <= 16.0) {
            application->userConfig.resolution = RT64::UserConfiguration::Resolution::Manual;
            application->userConfig.resolutionMultiplier = m;
            application->userConfig.downsampleMultiplier = 1;
            std::fprintf(stderr, "[psr-rt64] forced resolution = Manual, resolutionMultiplier = %.2f, downsampleMultiplier = 1\n", m);
        } else {
            std::fprintf(stderr, "[psr-rt64] PSR_RT64_RES_MULT='%s' rejected (need float in [1.0, 16.0])\n", r);
        }
    }
    // Supersampling control. RT64 renders at resolutionMultiplier and box-filters
    // down by downsampleMultiplier; the presented image is their ratio. To
    // supersample WITHOUT the non-native present-rescale that corrupts 2D menus,
    // pair a high PSR_RT64_RES_MULT with this so the OUTPUT matches the window's
    // native integer scale (e.g. at a 720p window: RES_MULT=6 + DOWNSAMPLE=2 =>
    // render 1440p, output 720p, 2x2 supersampled 3D, menus untouched).
    if (const char* d = std::getenv("PSR_RT64_DOWNSAMPLE")) {
        int dm = std::atoi(d);
        if (dm >= 1 && dm <= 8) {
            application->userConfig.downsampleMultiplier = dm;
            std::fprintf(stderr, "[psr-rt64] forced downsampleMultiplier = %d\n", dm);
        } else {
            std::fprintf(stderr, "[psr-rt64] PSR_RT64_DOWNSAMPLE='%s' rejected (need int in [1,8])\n", d);
        }
    }
    if (const char* f = std::getenv("PSR_RT64_FILTERING")) {
        if (!std::strcmp(f, "Nearest")) {
            application->userConfig.filtering = RT64::UserConfiguration::Filtering::Nearest;
            std::fprintf(stderr, "[psr-rt64] forced filtering = Nearest\n");
        } else if (!std::strcmp(f, "Linear")) {
            application->userConfig.filtering = RT64::UserConfiguration::Filtering::Linear;
            std::fprintf(stderr, "[psr-rt64] forced filtering = Linear\n");
        } else if (!std::strcmp(f, "AAPS")) {
            application->userConfig.filtering = RT64::UserConfiguration::Filtering::AntiAliasedPixelScaling;
            std::fprintf(stderr, "[psr-rt64] forced filtering = AntiAliasedPixelScaling\n");
        }
    }
    if (const char* u = std::getenv("PSR_RT64_UPSCALE2D")) {
        if (!std::strcmp(u, "Original")) {
            application->userConfig.upscale2D = RT64::UserConfiguration::Upscale2D::Original;
            std::fprintf(stderr, "[psr-rt64] forced upscale2D = Original\n");
        } else if (!std::strcmp(u, "ScaledOnly")) {
            application->userConfig.upscale2D = RT64::UserConfiguration::Upscale2D::ScaledOnly;
            std::fprintf(stderr, "[psr-rt64] forced upscale2D = ScaledOnly\n");
        } else if (!std::strcmp(u, "All")) {
            application->userConfig.upscale2D = RT64::UserConfiguration::Upscale2D::All;
            std::fprintf(stderr, "[psr-rt64] forced upscale2D = All\n");
        }
    }
    if (const char* aa = std::getenv("PSR_RT64_MSAA")) {
        using AA = RT64::UserConfiguration::Antialiasing;
        if (!std::strcmp(aa, "None") || !std::strcmp(aa, "0")) {
            application->userConfig.antialiasing = AA::None;
        } else if (!std::strcmp(aa, "MSAA2X") || !std::strcmp(aa, "2")) {
            application->userConfig.antialiasing = AA::MSAA2X;
        } else if (!std::strcmp(aa, "MSAA4X") || !std::strcmp(aa, "4")) {
            application->userConfig.antialiasing = AA::MSAA4X;
        } else if (!std::strcmp(aa, "MSAA8X") || !std::strcmp(aa, "8")) {
            application->userConfig.antialiasing = AA::MSAA8X;
        }
        std::fprintf(stderr, "[psr-rt64] forced antialiasing via PSR_RT64_MSAA='%s'\n", aa);
    }
    if (const char* tp = std::getenv("PSR_RT64_THREEPOINT")) {
        if (!std::strcmp(tp, "0") || !std::strcmp(tp, "false")) {
            application->userConfig.threePointFiltering = false;
            std::fprintf(stderr, "[psr-rt64] forced threePointFiltering = false\n");
        } else {
            application->userConfig.threePointFiltering = true;
            std::fprintf(stderr, "[psr-rt64] forced threePointFiltering = true\n");
        }
    }

    // WIDESCREEN EXPERIMENT (branch experiment/widescreen). Stadium is a 4:3
    // game; RT64 can widen the WORLD (3D) projection when its aspect scissor
    // spans the full framebuffer width (the same generic widening the Snap fix
    // rides, scoped-pillarbox logic already in rt64 main). The base config path
    // sets aspectRatio from config.ar_option but never sets the world
    // aspectTarget (only the ext/HUD path does), so Manual mode had no target.
    // PSR_ASPECT overrides both here, mirroring Snap's SNAP_ASPECT:
    //   PSR_ASPECT=original|4:3   -> Original  (no widening)
    //   PSR_ASPECT=expand|window  -> Expand    (widen to the output/window aspect)
    //   PSR_ASPECT=16:9|21:9|<f>  -> Manual + aspectTarget (deterministic widen)
    // NOTE: this widens the 3D scene only; the 2D/ortho HUD is governed by the
    // ext (hr_option) path and is expected to need separate handling.
    if (const char* asp = std::getenv("PSR_ASPECT")) {
        using AR = RT64::UserConfiguration::AspectRatio;
        std::string a = asp;
        for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (a == "original" || a == "4:3" || a == "4x3") {
            application->userConfig.aspectRatio = AR::Original;
            std::fprintf(stderr, "[psr-rt64] PSR_ASPECT: world aspect = Original (4:3)\n");
        } else if (a == "expand" || a == "window" || a == "auto") {
            application->userConfig.aspectRatio = AR::Expand;
            std::fprintf(stderr, "[psr-rt64] PSR_ASPECT: world aspect = Expand (fit output)\n");
        } else {
            // Parse "W:H" or a bare float into a target ratio.
            double target = 0.0;
            if (const char* colon = std::strchr(asp, ':')) {
                double w = std::atof(std::string(asp, colon).c_str());
                double h = std::atof(colon + 1);
                if (w > 0.0 && h > 0.0) target = w / h;
            } else {
                target = std::atof(asp);
            }
            if (target >= 0.1 && target <= 100.0) {
                application->userConfig.aspectRatio = AR::Manual;
                application->userConfig.aspectTarget = target;
                std::fprintf(stderr, "[psr-rt64] PSR_ASPECT: world aspect = Manual, target = %.6f\n", target);
            } else {
                std::fprintf(stderr, "[psr-rt64] PSR_ASPECT='%s' rejected (want original|expand|W:H|<float>)\n", asp);
            }
        }
    }
}

static ultramodern::renderer::SetupResult map_setup_result(RT64::Application::SetupResult rt64_result) {
    switch (rt64_result) {
        case RT64::Application::SetupResult::Success:                 return ultramodern::renderer::SetupResult::Success;
        case RT64::Application::SetupResult::DynamicLibrariesNotFound: return ultramodern::renderer::SetupResult::DynamicLibrariesNotFound;
        case RT64::Application::SetupResult::InvalidGraphicsAPI:      return ultramodern::renderer::SetupResult::InvalidGraphicsAPI;
        case RT64::Application::SetupResult::GraphicsAPINotFound:     return ultramodern::renderer::SetupResult::GraphicsAPINotFound;
        case RT64::Application::SetupResult::GraphicsDeviceNotFound:  return ultramodern::renderer::SetupResult::GraphicsDeviceNotFound;
    }
    fprintf(stderr, "Unhandled `RT64::Application::SetupResult` ?\n");
    assert(false);
    std::exit(EXIT_FAILURE);
}

static ultramodern::renderer::GraphicsApi map_graphics_api(RT64::UserConfiguration::GraphicsAPI api) {
    switch (api) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12:     return ultramodern::renderer::GraphicsApi::D3D12;
        case RT64::UserConfiguration::GraphicsAPI::Vulkan:    return ultramodern::renderer::GraphicsApi::Vulkan;
        case RT64::UserConfiguration::GraphicsAPI::Metal:     return ultramodern::renderer::GraphicsApi::Metal;
        case RT64::UserConfiguration::GraphicsAPI::Automatic: return ultramodern::renderer::GraphicsApi::Auto;
    }
    fprintf(stderr, "Unhandled `RT64::UserConfiguration::GraphicsAPI` ?\n");
    assert(false);
    std::exit(EXIT_FAILURE);
}

pokestadium::renderer::RT64Context::RT64Context(uint8_t* rdram,
                                                ultramodern::renderer::WindowHandle window_handle,
                                                bool debug) {
    static unsigned char dummy_rom_header[0x40];

    RT64::Application::Core appCore{};
#if defined(_WIN32)
    appCore.window = window_handle.window;
#elif defined(__linux__) || defined(__ANDROID__)
    appCore.window = window_handle;
#elif defined(__APPLE__)
    appCore.window.window = window_handle.window;
    appCore.window.view = window_handle.view;
#endif

    appCore.checkInterrupts = dummy_check_interrupts;

    appCore.HEADER = dummy_rom_header;
    appCore.RDRAM = rdram;
    appCore.DMEM = DMEM;
    appCore.IMEM = IMEM;

    appCore.MI_INTR_REG = &MI_INTR_REG;

    auto& dpc_regs = recomp::rdp::dp_registers();
    appCore.DPC_START_REG = &dpc_regs.start;
    appCore.DPC_END_REG = &dpc_regs.end;
    appCore.DPC_CURRENT_REG = &dpc_regs.current;
    appCore.DPC_STATUS_REG = &dpc_regs.status;
    appCore.DPC_CLOCK_REG = &dpc_regs.clock;
    appCore.DPC_BUFBUSY_REG = &dpc_regs.bufbusy;
    appCore.DPC_PIPEBUSY_REG = &dpc_regs.pipebusy;
    appCore.DPC_TMEM_REG = &dpc_regs.tmem;

    ultramodern::renderer::ViRegs* vi_regs = ultramodern::renderer::get_vi_regs();
    appCore.VI_STATUS_REG        = &vi_regs->VI_STATUS_REG;
    appCore.VI_ORIGIN_REG        = &vi_regs->VI_ORIGIN_REG;
    appCore.VI_WIDTH_REG         = &vi_regs->VI_WIDTH_REG;
    appCore.VI_INTR_REG          = &vi_regs->VI_INTR_REG;
    appCore.VI_V_CURRENT_LINE_REG = &vi_regs->VI_V_CURRENT_LINE_REG;
    appCore.VI_TIMING_REG        = &vi_regs->VI_TIMING_REG;
    appCore.VI_V_SYNC_REG        = &vi_regs->VI_V_SYNC_REG;
    appCore.VI_H_SYNC_REG        = &vi_regs->VI_H_SYNC_REG;
    appCore.VI_LEAP_REG          = &vi_regs->VI_LEAP_REG;
    appCore.VI_H_START_REG       = &vi_regs->VI_H_START_REG;
    appCore.VI_V_START_REG       = &vi_regs->VI_V_START_REG;
    appCore.VI_V_BURST_REG       = &vi_regs->VI_V_BURST_REG;
    appCore.VI_X_SCALE_REG       = &vi_regs->VI_X_SCALE_REG;
    appCore.VI_Y_SCALE_REG       = &vi_regs->VI_Y_SCALE_REG;

    RT64::ApplicationConfiguration appConfig;
    appConfig.useConfigurationFile = false;

    app = std::make_unique<RT64::Application>(appCore, appConfig);

    auto& cur_config = ultramodern::renderer::get_graphics_config();
    set_application_user_config(app.get(), cur_config);
    app->userConfig.developerMode = debug;
    // Force gbi depth branches to prevent LODs from kicking in.
    app->enhancementConfig.f3dex.forceBranch = true;
    // Scale LODs based on the output resolution.
    app->enhancementConfig.textureLOD.scale = true;

    switch (cur_config.api_option) {
        case ultramodern::renderer::GraphicsApi::D3D12:
            app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::D3D12; break;
        case ultramodern::renderer::GraphicsApi::Vulkan:
            app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Vulkan; break;
        case ultramodern::renderer::GraphicsApi::Metal:
            app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Metal; break;
        case ultramodern::renderer::GraphicsApi::Auto:
            app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Automatic; break;
    }

    // Register the SS Anne RmlUi overlay with RT64 before the application sets
    // up its render device — RT64 invokes the init hook (which needs the live
    // RenderInterface + RenderDevice) during setup, and the draw hook every
    // presented frame thereafter. (Phase 0 seam proof; see src/main/ui_seam.cpp.)
    pkmnstadium::ui_seam::install();

    uint32_t thread_id = 0;
#ifdef _WIN32
    thread_id = window_handle.thread_id;
#endif
    setup_result = map_setup_result(app->setup(thread_id));
    chosen_api = map_graphics_api(app->chosenGraphicsAPI);
    if (setup_result != ultramodern::renderer::SetupResult::Success) {
        app = nullptr;
        return;
    }

    app->setFullScreen(cur_config.wm_option == ultramodern::renderer::WindowMode::Fullscreen);

    if (app->device->getCapabilities().sampleLocations) {
        RT64::RenderSampleCounts color_sample_counts = app->device->getSampleCountsSupported(RT64::RenderFormat::R8G8B8A8_UNORM);
        RT64::RenderSampleCounts depth_sample_counts = app->device->getSampleCountsSupported(RT64::RenderFormat::D32_FLOAT);
        RT64::RenderSampleCounts common_sample_counts = color_sample_counts & depth_sample_counts;
        device_max_msaa = compute_max_supported_aa(common_sample_counts);
        sample_positions_supported = true;
    }
    else {
        device_max_msaa = RT64::UserConfiguration::Antialiasing::None;
        sample_positions_supported = false;
    }

    high_precision_fb_enabled = app->shaderLibrary->usesHDR;
}

pokestadium::renderer::RT64Context::~RT64Context() = default;

extern "C" void pkmnstadium_gdl_submit_snapshot(uint8_t* rdram, uint32_t dl_vaddr,
                                                uint64_t submit_seq);

void pokestadium::renderer::RT64Context::send_dl(const OSTask* task) {
    pkmnstadium::dbg::g_send_dl_count.fetch_add(1);
    switch (task->t.type) {
        case M_GFXTASK:    pkmnstadium::dbg::g_send_dl_gfx_count.fetch_add(1); break;
        case M_AUDTASK:    pkmnstadium::dbg::g_send_dl_audio_count.fetch_add(1); break;
        default:           pkmnstadium::dbg::g_send_dl_other_count.fetch_add(1); break;
    }
    /* Snapshot every G_DL push=1 (CALL) target's first 16 bytes BEFORE handing
     * the DL to RT64. submit_seq = current send_dl_gfx counter so we can
     * correlate with the walk-time snapshot the interpreter takes. */
    if (task->t.type == M_GFXTASK) {
        uint64_t submit_seq = pkmnstadium::dbg::g_send_dl_gfx_count.load();
        pkmnstadium_gdl_submit_snapshot(app->core.RDRAM,
                                        task->t.data_ptr & 0x3FFFFFF,
                                        submit_seq);
    }
    app->state->rsp->reset();
    app->interpreter->loadUCodeGBI(task->t.ucode & 0x3FFFFFF, task->t.ucode_data & 0x3FFFFFF, true);
    app->processDisplayLists(app->core.RDRAM, task->t.data_ptr & 0x3FFFFFF, 0, true);
}

void pokestadium::renderer::RT64Context::update_screen() {
    pkmnstadium::dbg::g_update_screen_count.fetch_add(1);
    app->updateScreen();
}

void pokestadium::renderer::RT64Context::shutdown() {
    if (app != nullptr) {
        app->end();
    }
}

bool pokestadium::renderer::RT64Context::update_config(
        const ultramodern::renderer::GraphicsConfig& old_config,
        const ultramodern::renderer::GraphicsConfig& new_config) {
    if (old_config == new_config) {
        return false;
    }
    if (new_config.wm_option != old_config.wm_option) {
        app->setFullScreen(new_config.wm_option == ultramodern::renderer::WindowMode::Fullscreen);
    }
    set_application_user_config(app.get(), new_config);
    app->updateUserConfig(true);
    if (new_config.msaa_option != old_config.msaa_option) {
        app->updateMultisampling();
    }
    return true;
}

void pokestadium::renderer::RT64Context::enable_instant_present() {
    app->enhancementConfig.presentation.mode = RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
    app->updateEnhancementConfig();
}

uint32_t pokestadium::renderer::RT64Context::get_display_framerate() const {
    return app->presentQueue->ext.sharedResources->swapChainRate;
}

float pokestadium::renderer::RT64Context::get_resolution_scale() const {
    constexpr int ReferenceHeight = 240;
    switch (app->userConfig.resolution) {
        case RT64::UserConfiguration::Resolution::WindowIntegerScale:
            if (app->sharedQueueResources->swapChainHeight > 0) {
                return std::max(float((app->sharedQueueResources->swapChainHeight + ReferenceHeight - 1) / ReferenceHeight), 1.0f);
            }
            return 1.0f;
        case RT64::UserConfiguration::Resolution::Manual:
            return float(app->userConfig.resolutionMultiplier);
        case RT64::UserConfiguration::Resolution::Original:
        default:
            return 1.0f;
    }
}

std::unique_ptr<ultramodern::renderer::RendererContext>
pokestadium::renderer::create_render_context(uint8_t* rdram,
                                             ultramodern::renderer::WindowHandle window_handle,
                                             bool developer_mode) {
    return std::make_unique<pokestadium::renderer::RT64Context>(rdram, window_handle, developer_mode);
}
