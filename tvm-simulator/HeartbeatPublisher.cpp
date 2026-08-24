#include "HeartbeatPublisher.hpp"
#include <chrono>
#include <iostream>
#include <mqtt/async_client.h>

HeartbeatPublisher::HeartbeatPublisher(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_topic) : m_publisher(std::make_unique<MqttPublisher>(p_server_adress, p_client_id)), m_topic(p_topic), m_client_id(p_client_id)
{
    m_thread = std::thread(&HeartbeatPublisher::run, this);
}

void HeartbeatPublisher::run() {
    std::unique_lock <std::mutex> lock(m_mutex);
    while(!m_cv.wait_for(lock, std::chrono::seconds(10), [this] {
            return (m_stop);})) {
        lock.unlock();
        m_publisher->publish(m_topic, m_client_id, 0, true);
        lock.lock();
    }
}

HeartbeatPublisher::~HeartbeatPublisher() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
}

// =====================================================================================

