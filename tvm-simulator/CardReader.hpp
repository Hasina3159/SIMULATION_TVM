#pragma once
#include <string>
#include <mqtt/async_client>

struct PaymentResult {
    bool success;
    std::string statut;        // "accepte" | "refuse_solde_insuffisant" | "refuse_compte_inconnu" | "timeout"
    double nouveau_solde;
};

class ICardReader {
public:
    virtual PaymentResult pay(const std::string &compte_id, double montant, const std::string &transaction_id);

    // Appelé par le dispatcher MQTT (ou directement par le callback Paho) quand une réponse arrive
    virtual void on_response_received(const std::string &payload);
};

class CardReader : public ICardReader {
private:
    std::string m_response_topic;
    std::string m_payment_topic;
    mqtt::async_client m_payment_client;
    mqtt::async_client m_response_client;
public:
    CardReader() = delete;
    CardReader(const CardReader &other) = delete;
    CardReader(CardReader &&other) = delete;
    CardReader &operator=(const CardReader &other) = delete;
    ~CardReader();
    CardReader(const std::string &payment_topic, const std::string &response_topic);
    PaymentResult pay(const std::string &compte_id, double montant, const std::string &transaction_id) override;
    void on_response_received(const std::string &payload) override;
}
