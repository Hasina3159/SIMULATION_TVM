#pragma once
#include <string>
#include <mqtt/async_client.h>
#include <unordered_map>
#include <vector>
#include <functional>

class Callback : public mqtt::callback {
private:
    std::unordered_map <std::string, std::function<void(mqtt::const_message_ptr)>> m_handlers; 
public:
    void connected (const std::string &cause) override;
    void connection_lost (const std::string &cause) override;
    void message_arrived (mqtt::const_message_ptr message) override;
    void delivery_complete (mqtt::delivery_token_ptr token) override;

    void register_handler(const std::string &topic, std::function<void(mqtt::const_message_ptr)> handler) {
        m_handlers[topic] = std::move(handler);
    }

    void unregister_handler(const std::string &topic) {
        m_handlers.erase(topic);
    }
};

class IMqttSubscriber {
public:
    virtual bool subscribe(const std::string& p_topic, int p_qos) = 0;
    virtual ~IMqttSubscriber() = default;
};

class MqttSubscriber : public IMqttSubscriber {
private:
    mqtt::async_client  m_client;
    std::string         m_will_message;
public:
    MqttSubscriber() = delete;
    MqttSubscriber(const MqttSubscriber& other) = delete;
    MqttSubscriber(MqttSubscriber&& other) noexcept = delete;
    MqttSubscriber &operator=(const MqttSubscriber &other) = delete;
    MqttSubscriber(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_will_topic, const std::string &p_will_message, int p_qos, bool p_retained,  Callback &p_callback);
    ~MqttSubscriber();
    bool subscribe(const std::string& p_topic, int p_qos) override;
};
