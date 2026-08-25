#pragma once
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include "SafeQueue.hpp"
#include "TvmSupervisor.hpp"
#include "CashDrawer.hpp"
#include "PrinterUnit.hpp"
#include "CardReader.hpp"
#include "MqttPublisher.hpp"
#include "MqttSubscriber.hpp"

// Compose et pilote un TVM complet a partir des commandes MQTT entrantes
// (tvm/{id}/commands/*), pour la version "solo" (un seul TVM, un seul
// catalogue de titres statique, pas de Collector de reservation/session).
class TvmOrchestrator {
private:
    std::string m_tvm_id;

    TvmSupervisor m_supervisor;
    CashDrawer m_cash_drawer;
    PrinterUnit m_printer;
    CardReader m_card_reader;
    std::unique_ptr<IMqttPublisher> m_state_publisher;

    // Catalogue statique de titres (prix en centimes, coherent avec CashDrawer).
    std::map<std::string, unsigned long long> m_catalogue = {
        {"ticket_unique", 200},
        {"carnet_10", 1600},
    };

    std::string m_selected_type;
    unsigned long long m_selected_price = 0;
    unsigned long long m_montant_insere = 0;
    std::atomic<unsigned long long> m_next_transaction_id{0};

    Callback m_commands_callback;
    std::unique_ptr<IMqttSubscriber> m_commands_subscriber;
    SafeQueue<mqtt::const_message_ptr> m_queue;
    std::thread m_worker;
    std::atomic<bool> m_stop{false};

    void worker_loop();
    void dispatch_command(mqtt::const_message_ptr message);

    void handle_selection_titre(const nlohmann::json &payload);
    void handle_inserer_espece(const nlohmann::json &payload);
    void handle_payer_carte(const nlohmann::json &payload);
    void handle_annuler();
    void finaliser_transaction(const std::string &mode_paiement, const std::string &compte_id);
    void annuler_transaction_en_cours();
    void reset_transaction();
    std::string generate_transaction_id();

    void publish_etat();
    void publish_caisse();
    void publish_vente(const std::string &mode_paiement, const std::string &compte_id, const std::string &transaction_id);
    void publish_erreur(const std::string &type, const std::string &message);

public:
    TvmOrchestrator() = delete;
    TvmOrchestrator(const TvmOrchestrator &other) = delete;
    TvmOrchestrator(TvmOrchestrator &&other) = delete;
    TvmOrchestrator &operator=(const TvmOrchestrator &other) = delete;
    ~TvmOrchestrator();

    TvmOrchestrator(const std::string &server_adress, const std::string &tvm_id);
};
