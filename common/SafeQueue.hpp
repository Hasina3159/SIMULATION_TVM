#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// File thread-safe : le callback Paho pousse (non-bloquant), un thread
// worker depile en attente (bloquant). stop() reveille le worker pour
// un arret propre, sans qu'il ait besoin d'attendre un element qui
// n'arrivera jamais.
template <typename T>
class SafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cv.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_stop; });
        if (m_queue.empty())
            return (std::nullopt);
        T value = std::move(m_queue.front());
        m_queue.pop();
        return (value);
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        m_cv.notify_all();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
};
