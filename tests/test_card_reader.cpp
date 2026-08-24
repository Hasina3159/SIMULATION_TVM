#include <gtest/gtest.h>
#include "CardReader.hpp"
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace {

// Fake IMqttPublisher : n'envoie rien sur le reseau, capture juste ce qui a
// ete "publie" pour que le test puisse construire une reponse correlee.
class FakeMqttPublisher : public IMqttPublisher {
public:
    bool publish(const std::string &topic, const std::string &payload, int qos, bool retained) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_topic = topic;
        m_last_payload = payload;
        (void)qos;
        (void)retained;
        m_publish_count++;
        m_cv.notify_all();
        return m_should_succeed;
    }

    // Bloque jusqu'a ce qu'une NOUVELLE publication ait eu lieu (pas une
    // ancienne deja vue), renvoie le correlation_id extrait de son payload.
    std::string wait_for_correlation_id() {
        std::unique_lock<std::mutex> lock(m_mutex);
        int baseline = m_publish_count;
        m_cv.wait(lock, [this, baseline] { return m_publish_count > baseline; });
        nlohmann::json demande = nlohmann::json::parse(m_last_payload);
        return demande.at("correlation_id").get<std::string>();
    }

    void fail_next_publish() { m_should_succeed = false; }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::string m_last_topic;
    std::string m_last_payload;
    int m_publish_count = 0;
    bool m_should_succeed = true;
};

const std::string PAYMENT_TOPIC = "tvm/test/paiement_compte/demande";
const std::string RESPONSE_TOPIC = "tvm/test/paiement_compte/reponse";

mqtt::const_message_ptr make_response(const std::string &correlation_id, const std::string &statut, double nouveau_solde) {
    nlohmann::json reponse = {
        {"correlation_id", correlation_id},
        {"statut", statut},
        {"nouveau_solde", nouveau_solde},
        {"timestamp", 0}
    };
    return mqtt::make_message(RESPONSE_TOPIC, reponse.dump(), 1, false);
}

}  // namespace

TEST(CardReaderTest, AcceptedPaymentReturnsSuccess) {
    auto fake = std::make_unique<FakeMqttPublisher>();
    FakeMqttPublisher &publisher = *fake;
    CardReader reader(std::move(fake), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::seconds(2));

    PaymentResult result;
    std::thread payer([&] { result = reader.pay("cl_92ab", 14.50, "tx_001"); });

    std::string correlation_id = publisher.wait_for_correlation_id();
    reader.on_response_received(make_response(correlation_id, "accepte", 12.30));

    payer.join();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statut, "accepte");
    EXPECT_DOUBLE_EQ(result.nouveau_solde, 12.30);
}

TEST(CardReaderTest, NoResponseTimesOutAndReturnsFailure) {
    CardReader reader(std::make_unique<FakeMqttPublisher>(), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::milliseconds(200));

    PaymentResult result = reader.pay("cl_92ab", 14.50, "tx_002");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.statut, "timeout");
}

TEST(CardReaderTest, ExplicitRefusalIsNotTreatedAsSuccess) {
    auto fake = std::make_unique<FakeMqttPublisher>();
    FakeMqttPublisher &publisher = *fake;
    CardReader reader(std::move(fake), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::seconds(2));

    PaymentResult result;
    std::thread payer([&] { result = reader.pay("cl_92ab", 14.50, "tx_003"); });

    std::string correlation_id = publisher.wait_for_correlation_id();
    reader.on_response_received(make_response(correlation_id, "refuse_solde_insuffisant", 5.0));

    payer.join();
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.statut, "refuse_solde_insuffisant");
}

TEST(CardReaderTest, PublishFailureIsReportedWithoutWaiting) {
    auto fake = std::make_unique<FakeMqttPublisher>();
    fake->fail_next_publish();
    CardReader reader(std::move(fake), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::seconds(5));

    PaymentResult result = reader.pay("cl_92ab", 14.50, "tx_004");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.statut, "erreur_publication");
}

TEST(CardReaderTest, UnknownCorrelationIdIsIgnoredWithoutCrash) {
    auto fake = std::make_unique<FakeMqttPublisher>();
    FakeMqttPublisher &publisher = *fake;
    CardReader reader(std::move(fake), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::milliseconds(300));

    PaymentResult result;
    std::thread payer([&] { result = reader.pay("cl_92ab", 14.50, "tx_005"); });

    publisher.wait_for_correlation_id();
    // Reponse pour un correlation_id qui n'a rien a voir : doit etre ignoree.
    reader.on_response_received(make_response("req-inconnu", "accepte", 99.0));

    payer.join();
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.statut, "timeout");
}

TEST(CardReaderTest, SequentialCallsDoNotLeakStateBetweenThem) {
    auto fake = std::make_unique<FakeMqttPublisher>();
    FakeMqttPublisher &publisher = *fake;
    CardReader reader(std::move(fake), PAYMENT_TOPIC, RESPONSE_TOPIC, std::chrono::seconds(2));

    PaymentResult first;
    std::thread payer1([&] { first = reader.pay("cl_92ab", 10.0, "tx_006"); });
    std::string id1 = publisher.wait_for_correlation_id();
    reader.on_response_received(make_response(id1, "accepte", 20.0));
    payer1.join();

    PaymentResult second;
    std::thread payer2([&] { second = reader.pay("cl_92ab", 5.0, "tx_007"); });
    std::string id2 = publisher.wait_for_correlation_id();
    reader.on_response_received(make_response(id2, "refuse_solde_insuffisant", 20.0));
    payer2.join();

    EXPECT_NE(id1, id2);
    EXPECT_TRUE(first.success);
    EXPECT_FALSE(second.success);
    EXPECT_EQ(second.statut, "refuse_solde_insuffisant");
}
