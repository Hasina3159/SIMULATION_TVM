#include "HeartbeatPublisher.hpp"
#include <chrono>
#include <iostream>
#include <mqtt/async_client.h>


MqttPublisher::MqttPublisher(const std::string &p_server_adress, const std::string &p_client_id) : m_client(p_server_adress, p_client_id)
{
    mqtt::connect_options conn_opts = mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(30))
        .automatic_reconnect(true)
        .clean_start(false)
        .clean_session(true)
        .finalize();
    m_client.connect(conn_opts)->wait();
}

MqttPublisher::~MqttPublisher() {
    try {
        m_client.disconnect()->wait();
    } catch (...) {}
}

bool MqttPublisher::publish (const std::string& topic, const std::string &payload, int qos, bool retained) {
    try {
        mqtt::message_ptr message = mqtt::make_message(topic, payload, qos, retained);
        m_client.publish(message);
    } catch (const mqtt::exception &exception) {
        return (false);
    }
    return (true);
}

// =====================================================================================

MqttSubscriber::MqttSubscriber(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_will_topic, const std::string &p_will_message, int p_qos, bool p_retained) : m_client(p_server_adress, p_client_id) {
    mqtt::connect_options conn_opts = mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(30))
        .automatic_reconnect(true)
        .clean_start(false)
        .clean_session(true)
        .will(mqtt::message(p_will_topic, p_will_message, p_qos, p_retained))
        .finalize();
    m_client.connect(conn_opts)->wait();
}

MqttSubscriber::~MqttSubscriber() {
    try {
        m_client.disconnect()->wait();
    } catch (...) {}
}

bool MqttSubscriber::subscribe(const std::string& p_topic, int p_qos) {
    try {
        m_client.subscribe(p_topic, p_qos);
        return (true);
    } catch (const mqtt::exception &exception) {
        return (false);
    }
}

// =====================================================================================

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

