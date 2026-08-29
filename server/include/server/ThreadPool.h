#pragma once

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// ThreadPool
//
// Fixed-size pool of worker threads that pull client-handling tasks from a
// shared queue. Used by Server to handle multiple connections concurrently.
//
// Design decisions:
//   - Fixed thread count: avoids unbounded thread creation under load.
//     If all threads are busy, new tasks wait in the queue.
//   - Unbounded queue: acceptable for game server where the OS limits the
//     number of simultaneous connections. If we add rate-limiting in a
//     future milestone, we can cap the queue at that point.
//   - Condition variable: workers sleep when queue is empty — zero CPU spin.
//   - Graceful shutdown: stop() sets m_stopping, notifies all workers via
//     notify_all(), then joins each thread. In-progress tasks complete
//     normally; queued tasks are discarded (clients will see a disconnect).
// ─────────────────────────────────────────────────────────────────────────────
class ThreadPool {
public:
    // Handler type: called once per accepted connection.
    // Receives the client socket fd and the remote address string.
    using Handler = std::function<void(int fd, const std::string& addr)>;

    // Create the pool and start 'numThreads' worker threads.
    ThreadPool(int numThreads, Handler handler);

    // Stop workers and join all threads.
    ~ThreadPool();

    // Non-copyable / non-movable
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a new accepted client to the work queue.
    // Thread-safe; may be called from the accept loop on the main thread.
    void submit(int fd, const std::string& addr);

    // Signal all workers to stop after finishing current tasks.
    void stop();

    // Diagnostics
    std::size_t pendingCount() const;
    int         threadCount()  const;

private:
    struct Task {
        int         fd;
        std::string addr;
    };

    void workerLoop();

    Handler                  m_handler;
    std::vector<std::thread> m_workers;

    mutable std::mutex       m_mutex;
    std::condition_variable  m_cv;
    std::queue<Task>         m_tasks;
    std::atomic<bool>        m_stopping{false};

    // Active task counter — how many workers are currently in handleClient
    std::atomic<int>         m_activeTasks{0};
};
