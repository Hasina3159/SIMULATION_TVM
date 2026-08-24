#include "CardReader.hpp"

CardReader::CardReader(const std::string &payment_topic, const std::string &response_topic) : m_response_topic(response_topic), m_payment_topic(payment_topic) {
    m_payment_client()

}

PaymentResult CardReader::pay(const std::string &compte_id, double montant, const std::string &transaction_id) override {

}

void CardReader::on_response_received(const std::string &payload) override {

}