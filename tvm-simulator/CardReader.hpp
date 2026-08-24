#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <atomic>
#include <mqtt/async_client.h>
#include "MqttPublisher.hpp"
#include "MqttSubscriber.hpp"
#include <nlohmann/json.hpp>

struct PaymentResult {
    bool success;
    std::string statut;        // "accepte" | "refuse_solde_insuffisant" | "refuse_compte_inconnu" | "timeout" | "erreur_publication"
    double nouveau_solde;
};

class ICardReader {
public:
    virtual PaymentResult pay(const std::string &compte_id, double montant, const std::string &transaction_id) = 0;
    virtual void on_response_received(mqtt::const_message_ptr message) = 0;
    virtual ~ICardReader() = default;
};

class CardReader : public ICardReader {
private:
    std::string m_response_topic;
    std::string m_payment_topic;
    int m_qos = 1;
    bool m_retained = false;
    std::chrono::milliseconds m_timeout;

    // Ordre de déclaration important : m_callback doit exister avant que
    // m_response_client ne soit construit (il lui passe une référence dessus).
    std::unique_ptr<IMqttPublisher> m_payment_client;
    Callback m_callback;
    std::unique_ptr<IMqttSubscriber> m_response_client;

    std::atomic<unsigned long long> m_next_id{0};

    // Un seul emplacement en attente, pas une map : un TVM ne traite jamais
    // deux paiements carte en même temps (voir CardReader_NOTES.md, section 4).
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::string m_pending_correlation_id;
    std::optional<PaymentResult> m_response;

    std::string generate_correlation_id();

public:
    CardReader() = delete;
    CardReader(const CardReader &other) = delete;
    CardReader(CardReader &&other) = delete;
    CardReader &operator=(const CardReader &other) = delete;
    ~CardReader() = default;

    // Constructeur "production" : cree ses propres connexions MQTT reelles.
    CardReader(const std::string &server_adress, const std::string &client_id,
               const std::string &payment_topic, const std::string &response_topic,
               std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Constructeur "test" : injecte un IMqttPublisher deja construit (fake/mock),
    // ne cree aucune connexion d'ecoute reelle. Pour simuler une reponse du
    // Collector dans un test, appeler on_response_received(...) directement.
    CardReader(std::unique_ptr<IMqttPublisher> payment_client,
               const std::string &payment_topic, const std::string &response_topic,
               std::chrono::milliseconds timeout = std::chrono::seconds(5));

    PaymentResult pay(const std::string &compte_id, double montant, const std::string &transaction_id) override;
    void on_response_received(mqtt::const_message_ptr message) override;
};
