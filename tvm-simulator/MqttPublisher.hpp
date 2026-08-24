#pragma once
#include <string>
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