/*
 * trace.h — TRACE_ENTRY / TRACE_RETURN hooks for trace_mode = true.
 *
 * N64Recomp's recompiler emits literal `TRACE_ENTRY()` and
 * `TRACE_RETURN()` at the start/end of every recompiled function
 * when game.toml has `trace_mode = true`. The header is auto-
 * included into every generated funcs_N.c via config.recomp_include.
 *
 * These hooks push (function_name) into a lock-free ring buffer
 * exposed via the TCP debug server's `trace_recent` command. Lets
 * us see — without modifying the engine or generated output —
 * exactly which game functions are executing right now. Critical
 * for finding "where is the boot stuck?" without random trial
 * fixes.
 *
 * Per project principles (PRINCIPLES.md #12): not stubs. Real
 * function calls into a real ring writer; the recorded data is
 * used for diagnosis, not for simulating behavior.
 */

#ifndef PKMNSTADIUM_TRACE_H
#define PKMNSTADIUM_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void pkmnstadium_trace_entry(const char *func);
void pkmnstadium_trace_return(const char *func);
void pkmnstadium_trace_entry_ctx(const char *func, const void *ctx);
void pkmnstadium_trace_return_ctx(const char *func, const void *ctx);
void pkmnstadium_gbtower_state_trace(unsigned char *rdram, unsigned int tag,
                                     unsigned int s0, const void *ctx);
void pkmnstadium_gbtower_queue_audio(unsigned char *rdram,
                                     unsigned int audio_addr,
                                     unsigned int byte_count);

/* Voluntary-preemption tick (defined in N64ModernRuntime
 * ultramodern/src/scheduler_tick.cpp, extern "C"). One relaxed atomic
 * load on the fast path; yields only when the host monitor has flagged
 * the current game thread as stuck >200ms. */
void ultramodern_scheduler_tick(void);

#ifdef __cplusplus
}
#endif

// Engine emits these as bare `TRACE_ENTRY()` / `TRACE_RETURN()`
// without a trailing `;` — bake the terminator into the macro
// so each expansion is a complete statement on its own. (do/while(0)
// would still need an outer `;`.) Trailing `;` is fine even if
// the engine ever does emit one — `;;` is a valid empty statement.
#ifndef PKMNSTADIUM_TRACE_HOOKS
#define PKMNSTADIUM_TRACE_HOOKS 0
#endif

#if PKMNSTADIUM_TRACE_HOOKS
#define TRACE_ENTRY()  pkmnstadium_trace_entry_ctx(__func__, ctx);
#define TRACE_RETURN() pkmnstadium_trace_return_ctx(__func__, ctx);
#else
#define TRACE_ENTRY()
#define TRACE_RETURN()
#endif

// Emitted by the recompiler on loop back-edges (trace_mode). Pure
// preemption checkpoint — NO ring logging (a hot loop would flood the
// trace ring); just the cheap scheduler tick so tight intra-function
// spin loops become yield points.
#define TRACE_LOOP()   ultramodern_scheduler_tick();

#endif
