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


class IMqttSubscriber {
public:
    virtual bool subscribe(const std::string& p_topic, int p_qos) = 0;
    virtual ~IMqttSubscriber() = default;
};

class MqttSubscriber : public IMqttSubscriber {
private:
    mqtt::async_client m_client;
    std::string m_will_message;
public:
    MqttSubscriber() = delete;
    MqttSubscriber(const MqttSubscriber& other) = delete;
    MqttSubscriber(MqttSubscriber&& other) noexcept = delete;
    MqttSubscriber &operator=(const MqttSubscriber &other) = delete;
    MqttSubscriber(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_will_topic, const std::string &p_will_message, int p_qos, bool p_retained);
    ~MqttSubscriber();
    bool subscribe(const std::string& p_topic, int p_qos) override;
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