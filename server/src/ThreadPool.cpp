#include "server/ThreadPool.h"
#include "server/Logger.h"

#include <unistd.h>

// ── Construction / destruction ────────────────────────────────────────────────

ThreadPool::ThreadPool(int numThreads, Handler handler)
    : m_handler(std::move(handler))
{
    if (numThreads <= 0) numThreads = 1;

    m_workers.reserve(static_cast<std::size_t>(numThreads));
    for (int i = 0; i < numThreads; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }

    Logger::info("ThreadPool started: threads=" + std::to_string(numThreads));
}

ThreadPool::~ThreadPool() {
    stop();
}

// ── Public interface ──────────────────────────────────────────────────────────

void ThreadPool::submit(int fd, const std::string& addr) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping.load()) {
            // Pool is shutting down — refuse new work, close the fd
            ::close(fd);
            return;
        }
        m_tasks.push({fd, addr});
    }
    m_cv.notify_one(); // wake exactly one waiting worker
}

void ThreadPool::stop() {
    m_stopping.store(true);
    m_cv.notify_all(); // wake all workers so they can exit their wait loop

    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();
}

std::size_t ThreadPool::pendingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.size();
}

int ThreadPool::threadCount() const {
    return static_cast<int>(m_workers.size());
}

// ── Worker loop ───────────────────────────────────────────────────────────────

void ThreadPool::workerLoop() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait until there is work OR we are stopping
            m_cv.wait(lock, [this] {
                return !m_tasks.empty() || m_stopping.load();
            });

            if (m_stopping.load() && m_tasks.empty()) {
                // No more work and shutting down — exit thread
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        // Handle the client outside the lock so other workers can proceed
        ++m_activeTasks;
        try {
            m_handler(task.fd, task.addr);
        } catch (const std::exception& ex) {
            Logger::error("ThreadPool worker exception: " + std::string(ex.what()));
        } catch (...) {
            Logger::error("ThreadPool worker: unknown exception");
        }
        --m_activeTasks;
    }
}
