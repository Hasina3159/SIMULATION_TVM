#include "HeartbeatPublisher.hpp"
#include <chrono>
#include <iostream>
#include <mqtt/async_client.h>

HeartbeatPublisher::HeartbeatPublisher(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_topic) {
    m_publisher = std::make_unique<MqttPublisher>(p_server_adress, p_client_id);
    m_publisher->publish(p_topic, "[" + p_client_id + "]", 0, true);
}

HeartbeatPublisher::~HeartbeatPublisher() {
}
// ==========================================================================================

MqttPublisher::MqttPublisher(const std::string &p_server_adress, const std::string &p_client_id) : m_client(p_server_adress, p_client_id)
{
    mqtt::connect_options conn_opts = mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(30))
        .automatic_reconnect(true)
        .clean_start(false)
        .clean_session(true)
        .finalize();

    /*
        try {
            client.connect(conn_opts_builder.finalize())->wait();
        } catch (const mqtt::exception &exception) {
            std::cerr << "Error : " << exception.what() << std::endl; 
        }
            je pense oft laisser expres un exception en cas d'erreur
    */
    m_client.connect(conn_opts)->wait();
}

MqttPublisher::~MqttPublisher() {
    m_client.disconnect()->wait();
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