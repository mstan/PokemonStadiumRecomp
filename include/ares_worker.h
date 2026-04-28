/*
 * ares_worker.h — single-threaded Ares oracle owner.
 *
 * Why this exists:
 *   Ares uses libco (cooperative threading) with thread_local state for
 *   co_active_handle. When system.run() is called from any thread other
 *   than the one that originally created the cothreads, libco's
 *   thread_local handle is inconsistent and the CPU/RSP cothreads never
 *   actually advance — observed empirically as "RSP per-instruction
 *   trace ring stays at 0 after dozens of frames" when ares_step_frame
 *   is called from the TCP debug-server worker thread.
 *
 * Solution:
 *   Pin all Ares calls to a single dedicated worker thread, started
 *   early in main(). Other threads (TCP debug server, etc.) post
 *   `std::function<void()>` jobs to the worker and wait on a future.
 *
 * Lifecycle:
 *   - ares_worker::start() — call once early in main, before the debug
 *     server starts. Spawns the dedicated thread.
 *   - ares_worker::dispatch(fn) — synchronous: posts fn, blocks until
 *     it returns, returns its result. Safe to call from any thread.
 *   - ares_worker::shutdown() — stops the worker thread cleanly.
 */

#ifndef PKMNSTADIUM_ARES_WORKER_H
#define PKMNSTADIUM_ARES_WORKER_H

#include <future>
#include <functional>

namespace pkmnstadium {
namespace ares_worker {

void start();
void shutdown();

// Run `fn` on the dedicated Ares worker thread, blocking the caller
// until completion. Returns whatever `fn` returns.
template <typename F>
auto dispatch(F&& fn) -> decltype(fn()) {
    using Ret = decltype(fn());
    std::promise<Ret> promise;
    auto fut = promise.get_future();
    extern void post_job(std::function<void()>);
    if constexpr (std::is_void_v<Ret>) {
        post_job([fn = std::forward<F>(fn), &promise]() mutable {
            fn();
            promise.set_value();
        });
        fut.get();
    } else {
        post_job([fn = std::forward<F>(fn), &promise]() mutable {
            promise.set_value(fn());
        });
        return fut.get();
    }
}

// Lower-level primitive used by the dispatch template.
void post_job(std::function<void()> job);

} // namespace ares_worker
} // namespace pkmnstadium

#endif
