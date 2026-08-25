#include "MqttListener.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

MqttListener::MqttListener(const std::string &server_adress, const std::string &client_id, ComptesManager &comptes)
    : m_comptes(comptes),
      m_publisher(std::make_unique<MqttPublisher>(server_adress, client_id + "_pub")),
      m_subscriber(std::make_unique<MqttSubscriber>(
          server_adress, client_id + "_sub",
          client_id + "/lwt", "offline", 1, true,
          m_callback))
{
    m_callback.register_handler([this](mqtt::const_message_ptr message) {
        m_queue.push(message);
    });
    m_subscriber->subscribe("tvm/+/paiement_compte/demande", 1);
    m_worker = std::thread(&MqttListener::worker_loop, this);
}

MqttListener::~MqttListener() {
    m_stop = true;
    m_queue.stop();
    if (m_worker.joinable())
        m_worker.join();
}

void MqttListener::worker_loop() {
    while (!m_stop) {
        std::optional<mqtt::const_message_ptr> message = m_queue.pop();
        if (!message.has_value())
            continue;
        handle_demande(*message);
    }
}

void MqttListener::handle_demande(mqtt::const_message_ptr message) {
    try {
        const std::string &topic = message->get_topic();
        // topic attendu : tvm/{tvm_id}/paiement_compte/demande
        size_t first_slash = topic.find('/');
        size_t second_slash = topic.find('/', first_slash + 1);
        if (first_slash == std::string::npos || second_slash == std::string::npos)
            throw std::runtime_error("topic inattendu : " + topic);
        std::string tvm_id = topic.substr(first_slash + 1, second_slash - first_slash - 1);

        nlohmann::json demande = nlohmann::json::parse(message->get_payload_str());
        std::string correlation_id = demande.at("correlation_id").get<std::string>();
        std::string compte_id = demande.at("compte_id").get<std::string>();
        double montant = demande.at("montant").get<double>();
        std::string transaction_id = demande.value("transaction_id", "");

        DebitResult debit = m_comptes.debiter(compte_id, montant, transaction_id);

        nlohmann::json reponse = {
            {"correlation_id", correlation_id},
            {"statut", debit.statut},
            {"nouveau_solde", debit.nouveau_solde},
            {"timestamp", 0}
        };

        std::string response_topic = "tvm/" + tvm_id + "/paiement_compte/reponse";
        m_publisher->publish(response_topic, reponse.dump(), 1, false);
    } catch (const std::exception &e) {
        std::cerr << "MqttListener: demande malformee ignoree (" << e.what() << ")" << std::endl;
    }
}
