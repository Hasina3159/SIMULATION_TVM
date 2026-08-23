#pragma once
#include <string>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <mqtt/async_client.h>

class IMqttPublisher {
public:
    virtual bool publish(const std::string& topic, const std::string &payload, int qos, bool retained) = 0;
    virtual ~IMqttPublisher() = default;
};


class MqttPublisher : public IMqttPublisher {
private:
    mqtt::async_client m_client;
public:
    MqttPublisher() = delete;
    MqttPublisher(const MqttPublisher& other) = delete;
    MqttPublisher(MqttPublisher&& other) noexcept = delete;
    MqttPublisher &operator=(const MqttPublisher &other) = delete;
    MqttPublisher(const std::string &p_server_adress, const std::string &p_client_id);
    ~MqttPublisher();
    bool publish(const std::string& topic, const std::string &payload, int qos, bool retained) override;
};


class HeartbeatPublisher
{
private:
    std::unique_ptr<IMqttPublisher> m_publisher;
    std::string m_topic;
    std::string m_client_id;
    std::mutex m_mutex;
    std::thread m_thread;
    std::condition_variable m_cv;
    bool m_stop = false;

    void run();
public:
    HeartbeatPublisher() = delete;
    HeartbeatPublisher(const std::string &p_server_adress, const std::string &p_client_id, const std::string &topic);
    HeartbeatPublisher(const HeartbeatPublisher &other) = delete;
    HeartbeatPublisher(HeartbeatPublisher &&other) noexcept = delete;
    HeartbeatPublisher &operator=(const HeartbeatPublisher &other) = delete;
    ~HeartbeatPublisher();
};