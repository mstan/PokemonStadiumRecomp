/*
 * ares_worker.cpp — see include/ares_worker.h for the rationale.
 */

#include "ares_worker.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace pkmnstadium {
namespace ares_worker {

namespace {

std::thread                       g_thread;
std::mutex                        g_mutex;
std::condition_variable           g_cv;
std::deque<std::function<void()>> g_queue;
std::atomic<bool>                 g_running{false};

void worker_main() {
    std::fprintf(stderr,
        "[ares_worker] thread started (id=%u)\n",
        (unsigned)std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::fflush(stderr);
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait(lock, [] { return !g_queue.empty() || !g_running.load(); });
            if (!g_running.load() && g_queue.empty()) break;
            job = std::move(g_queue.front());
            g_queue.pop_front();
        }
        job();
    }
    std::fprintf(stderr, "[ares_worker] thread exiting\n");
    std::fflush(stderr);
}

} // namespace

void start() {
    if (g_running.exchange(true)) return;
    g_thread = std::thread(worker_main);
}

void shutdown() {
    if (!g_running.exchange(false)) return;
    g_cv.notify_all();
    if (g_thread.joinable()) g_thread.join();
}

void post_job(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_queue.push_back(std::move(job));
    }
    g_cv.notify_one();
}

} // namespace ares_worker
} // namespace pkmnstadium
