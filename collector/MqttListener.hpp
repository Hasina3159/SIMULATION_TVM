#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mqtt/async_client.h>
#include "SafeQueue.hpp"
#include "MqttPublisher.hpp"
#include "MqttSubscriber.hpp"
#include "ComptesManager.hpp"

// Ecoute tvm/+/paiement_compte/demande, debite via ComptesManager, republie
// la reponse corretlee sur tvm/{tvm_id}/paiement_compte/reponse. Le callback
// Paho ne fait que pousser dans la SafeQueue (jamais bloquant) ; un thread
// worker depile et fait le vrai travail (parsing JSON, SQLite, publication).
class MqttListener {
private:
    ComptesManager &m_comptes;
    Callback m_callback;
    std::unique_ptr<IMqttPublisher> m_publisher;
    std::unique_ptr<IMqttSubscriber> m_subscriber;
    SafeQueue<mqtt::const_message_ptr> m_queue;
    std::thread m_worker;
    std::atomic<bool> m_stop{false};

    void worker_loop();
    void handle_demande(mqtt::const_message_ptr message);

public:
    MqttListener() = delete;
    MqttListener(const MqttListener &other) = delete;
    MqttListener(MqttListener &&other) = delete;
    MqttListener &operator=(const MqttListener &other) = delete;
    ~MqttListener();

    MqttListener(const std::string &server_adress, const std::string &client_id, ComptesManager &comptes);
};
