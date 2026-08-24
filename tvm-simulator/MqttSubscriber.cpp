#include "MqttSubscriber.hpp"

MqttSubscriber::MqttSubscriber(const std::string &p_server_adress, const std::string &p_client_id, const std::string &p_will_topic, const std::string &p_will_message, int p_qos, bool p_retained,  Callback &p_callback) : m_client(p_server_adress, p_client_id) {
    mqtt::connect_options conn_opts = mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(30))
        .automatic_reconnect(true)
        .clean_start(false)
        .clean_session(true)
        .will(mqtt::message(p_will_topic, p_will_message, p_qos, p_retained))
        .finalize();
    m_client.set_callback(p_callback);
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

//=============================================================================

void Callback::message_arrived(mqtt::const_message_ptr message)
{
    auto it = m_handlers.find(message->get_topic());
    if (it != m_handlers.end())
        it->second(message);
}

void Callback::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
}

void Callback::connected(const std::string &cause) {
    (void) cause;
}

void Callback::connection_lost(const std::string &cause) {
    (void) cause;
}
