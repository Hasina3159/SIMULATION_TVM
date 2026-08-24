#include "MqttPublisher.hpp"

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