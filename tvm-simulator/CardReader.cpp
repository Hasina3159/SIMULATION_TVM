#include "CardReader.hpp"
#include <iostream>

CardReader::CardReader(const std::string &server_adress, const std::string &client_id,
                        const std::string &payment_topic, const std::string &response_topic,
                        std::chrono::milliseconds timeout)
    : m_response_topic(response_topic),
      m_payment_topic(payment_topic),
      m_timeout(timeout),
      m_payment_client(std::make_unique<MqttPublisher>(server_adress, client_id)),
      // client_id distinct pour la connexion d'écoute : deux mqtt::async_client
      // avec le même client_id sur le même broker se feraient mutuellement
      // déconnecter par le broker (contrainte du protocole MQTT).
      m_response_client(std::make_unique<MqttSubscriber>(
          server_adress, client_id + "_sub",
          client_id + "/lwt", "offline", 1, true,
          m_callback))
{
    m_callback.register_handler(response_topic, [this](mqtt::const_message_ptr message) {
        on_response_received(message);
    });
    m_response_client->subscribe(response_topic, m_qos);
}

CardReader::CardReader(std::unique_ptr<IMqttPublisher> payment_client,
                        const std::string &payment_topic, const std::string &response_topic,
                        std::chrono::milliseconds timeout)
    : m_response_topic(response_topic),
      m_payment_topic(payment_topic),
      m_timeout(timeout),
      m_payment_client(std::move(payment_client)),
      m_response_client(nullptr)
{
    // Pas de vraie connexion d'ecoute ici : les tests simulent une reponse
    // du Collector en appelant on_response_received(...) directement.
}

std::string CardReader::generate_correlation_id() {
    return ("req-" + std::to_string(++m_next_id));
}

PaymentResult CardReader::pay(const std::string &compte_id, double montant, const std::string &transaction_id) {
    std::string correlation_id = generate_correlation_id();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_correlation_id = correlation_id;
        m_response.reset();
    }

    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    nlohmann::json demande = {
        {"correlation_id", correlation_id},
        {"compte_id", compte_id},
        {"montant", montant},
        {"transaction_id", transaction_id},
        {"timestamp", timestamp}
    };

    if (!m_payment_client->publish(m_payment_topic, demande.dump(), m_qos, m_retained)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_correlation_id.clear();
        return (PaymentResult{false, "erreur_publication", 0.0});
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    bool arrived = m_cv.wait_for(lock, m_timeout, [this] { return m_response.has_value(); });

    PaymentResult result = arrived ? *m_response : PaymentResult{false, "timeout", 0.0};
    m_pending_correlation_id.clear();
    m_response.reset();
    return (result);
}

void CardReader::on_response_received(mqtt::const_message_ptr message) {
    PaymentResult result;
    try {
        nlohmann::json reponse = nlohmann::json::parse(message->get_payload_str());
        std::string correlation_id = reponse.at("correlation_id").get<std::string>();

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pending_correlation_id.empty() || correlation_id != m_pending_correlation_id)
            return; // reponse tardive ou hors sujet -> ignoree, jamais de crash

        result.statut = reponse.at("statut").get<std::string>();
        result.success = (result.statut == "accepte");
        result.nouveau_solde = reponse.value("nouveau_solde", 0.0);
        m_response = result;
    } catch (const std::exception &e) {
        std::cerr << "CardReader: reponse malformee ignoree (" << e.what() << ")" << std::endl;
        return;
    }
    m_cv.notify_one();
}
