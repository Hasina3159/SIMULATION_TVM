#include "TvmOrchestrator.hpp"
#include <iostream>
#include <chrono>

namespace {

long long now_timestamp() {
    return (std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string state_to_string(TvmState state) {
    switch (state) {
        case TvmState::IDLE: return ("IDLE");
        case TvmState::SELECTING_TICKET: return ("SELECTING_TICKET");
        case TvmState::SUMMARIZING_ORDER: return ("SUMMARIZING_ORDER");
        case TvmState::AWAITING_PAYMENT: return ("AWAITING_PAYMENT");
        case TvmState::VALIDATING_PAYMENT: return ("VALIDATING_PAYMENT");
        case TvmState::PRINTING: return ("PRINTING");
        case TvmState::DISPENSING: return ("DISPENSING");
        case TvmState::ERROR: return ("ERROR");
    }
    return ("INCONNU");
}

}  // namespace

TvmOrchestrator::TvmOrchestrator(const std::string &server_adress, const std::string &tvm_id)
    : m_tvm_id(tvm_id),
      m_cash_drawer(std::map<unsigned long long, unsigned int>{
          {10, 20}, {20, 20}, {50, 20}, {100, 20}, {200, 20},
          {500, 20}, {1000, 20}, {2000, 20}, {5000, 20}}),
      m_card_reader(server_adress, tvm_id + "_card",
                    "tvm/" + tvm_id + "/paiement_compte/demande",
                    "tvm/" + tvm_id + "/paiement_compte/reponse"),
      m_state_publisher(std::make_unique<MqttPublisher>(server_adress, tvm_id + "_state")),
      m_commands_subscriber(std::make_unique<MqttSubscriber>(
          server_adress, tvm_id + "_cmd",
          "tvm/" + tvm_id + "/etat", R"({"etat":"hors_ligne"})", 1, true,
          m_commands_callback))
{
    m_commands_callback.register_handler([this](mqtt::const_message_ptr message) {
        m_queue.push(message);
    });
    m_commands_subscriber->subscribe("tvm/" + m_tvm_id + "/commands/#", 1);
    m_worker = std::thread(&TvmOrchestrator::worker_loop, this);

    publish_etat();
    publish_caisse();
    std::cout << "TvmOrchestrator '" << m_tvm_id << "' pret, en attente de commandes." << std::endl;
}

TvmOrchestrator::~TvmOrchestrator() {
    m_stop = true;
    m_queue.stop();
    if (m_worker.joinable())
        m_worker.join();
}

void TvmOrchestrator::worker_loop() {
    while (!m_stop) {
        std::optional<mqtt::const_message_ptr> message = m_queue.pop();
        if (!message.has_value())
            continue;
        dispatch_command(*message);
    }
}

void TvmOrchestrator::dispatch_command(mqtt::const_message_ptr message) {
    try {
        const std::string &topic = message->get_topic();
        const std::string marker = "/commands/";
        size_t position = topic.find(marker);
        if (position == std::string::npos)
            throw std::runtime_error("topic de commande inattendu : " + topic);
        std::string command_name = topic.substr(position + marker.size());

        nlohmann::json payload = message->get_payload_str().empty()
            ? nlohmann::json::object()
            : nlohmann::json::parse(message->get_payload_str());

        if (command_name == "selection_titre")
            handle_selection_titre(payload);
        else if (command_name == "inserer_espece")
            handle_inserer_espece(payload);
        else if (command_name == "payer_carte")
            handle_payer_carte(payload);
        else if (command_name == "annuler")
            handle_annuler();
        else
            publish_erreur("commande_inconnue", command_name);
    } catch (const std::exception &e) {
        std::cerr << "TvmOrchestrator: commande malformee ignoree (" << e.what() << ")" << std::endl;
        publish_erreur("commande_malformee", e.what());
    }
}

void TvmOrchestrator::handle_selection_titre(const nlohmann::json &payload) {
    if (m_supervisor.get_state() != TvmState::IDLE) {
        publish_erreur("etat_invalide", "selection_titre refusee, le TVM n'est pas IDLE");
        return;
    }

    std::string type_titre = payload.at("type_titre").get<std::string>();
    unsigned int quantite = payload.value("quantite", 1);

    auto it = m_catalogue.find(type_titre);
    if (it == m_catalogue.end()) {
        publish_erreur("titre_inconnu", type_titre);
        return;
    }

    m_selected_type = type_titre;
    m_selected_price = it->second * quantite;
    m_montant_insere = 0;

    m_supervisor.set_state(TvmState::SELECTING_TICKET);
    m_supervisor.set_state(TvmState::SUMMARIZING_ORDER);
    m_supervisor.set_state(TvmState::AWAITING_PAYMENT);
    publish_etat();
}

void TvmOrchestrator::handle_inserer_espece(const nlohmann::json &payload) {
    if (m_supervisor.get_state() != TvmState::AWAITING_PAYMENT) {
        publish_erreur("etat_invalide", "inserer_espece refusee hors AWAITING_PAYMENT");
        return;
    }

    unsigned long long denomination = payload.at("denomination").get<unsigned long long>();
    Denomination piece{denomination, 1};
    if (m_cash_drawer.add_cash(piece) != CashDrawerErrorStatus::OK) {
        publish_erreur("denomination_invalide", std::to_string(denomination));
        return;
    }
    m_montant_insere += denomination;
    publish_caisse();

    if (m_montant_insere < m_selected_price)
        return;

    unsigned long long a_rendre = m_montant_insere - m_selected_price;
    CashDrawerErrorStatus statut_rendu;
    m_cash_drawer.retire_cash(a_rendre, statut_rendu);

    if (statut_rendu != CashDrawerErrorStatus::OK) {
        // Rendu impossible : on annule integralement, les especes inserees
        // sont rendues au client (CDC section 5.3 / piege 13.2).
        m_cash_drawer.rollback();
        publish_erreur("stock_monnaie_insuffisant", "rendu impossible, especes rendues integralement");
        m_supervisor.set_state(TvmState::IDLE);
        reset_transaction();
        publish_etat();
        return;
    }

    m_supervisor.set_state(TvmState::VALIDATING_PAYMENT);
    publish_etat();
    double monnaie_rendue = static_cast<double>(a_rendre) / 100.0;
    finaliser_transaction("especes", "", monnaie_rendue);
}

void TvmOrchestrator::handle_payer_carte(const nlohmann::json &payload) {
    if (m_supervisor.get_state() != TvmState::AWAITING_PAYMENT) {
        publish_erreur("etat_invalide", "payer_carte refusee hors AWAITING_PAYMENT");
        return;
    }

    std::string compte_id = payload.at("compte_id").get<std::string>();
    std::string transaction_id = generate_transaction_id();

    m_supervisor.set_state(TvmState::VALIDATING_PAYMENT);
    publish_etat();

    double montant_euros = static_cast<double>(m_selected_price) / 100.0;
    PaymentResult result = m_card_reader.pay(compte_id, montant_euros, transaction_id);

    if (!result.success) {
        m_supervisor.set_state(TvmState::ERROR);
        publish_erreur("paiement_refuse", result.statut);
        publish_etat();
        m_supervisor.set_state(TvmState::IDLE);
        reset_transaction();
        publish_etat();
        return;
    }

    finaliser_transaction("carte", compte_id, 0.0);
}

void TvmOrchestrator::handle_annuler() {
    TvmState etat_courant = m_supervisor.get_state();
    if (etat_courant == TvmState::IDLE)
        return;

    double montant_rendu = 0.0;
    if (etat_courant == TvmState::AWAITING_PAYMENT && m_montant_insere > 0) {
        m_cash_drawer.rollback();
        montant_rendu = static_cast<double>(m_montant_insere) / 100.0;
        m_montant_insere = 0;
    }

    if (!m_supervisor.set_state(TvmState::IDLE)) {
        // Annulation demandee en pleine validation/impression : pas de
        // transition directe vers IDLE prevue a ce stade, on ignore.
        return;
    }
    reset_transaction();
    if (montant_rendu > 0.0)
        publish_annulation(montant_rendu);
    publish_etat();
}

void TvmOrchestrator::finaliser_transaction(const std::string &mode_paiement, const std::string &compte_id, double monnaie_rendue) {
    m_supervisor.set_state(TvmState::PRINTING);
    publish_etat();

    std::string ticket = m_selected_type + " (" + mode_paiement + ")";
    if (!m_printer.print(ticket)) {
        m_supervisor.set_state(TvmState::ERROR);
        publish_erreur("bourrage_papier", ticket);
        publish_etat();
        m_supervisor.set_state(TvmState::IDLE);
        reset_transaction();
        publish_etat();
        return;
    }

    m_supervisor.set_state(TvmState::DISPENSING);
    publish_etat();
    publish_vente(mode_paiement, compte_id, generate_transaction_id(), monnaie_rendue);
    publish_caisse();

    m_supervisor.set_state(TvmState::IDLE);
    reset_transaction();
    publish_etat();
}

void TvmOrchestrator::reset_transaction() {
    m_selected_type.clear();
    m_selected_price = 0;
    m_montant_insere = 0;
}

std::string TvmOrchestrator::generate_transaction_id() {
    return ("tx-" + m_tvm_id + "-" + std::to_string(++m_next_transaction_id));
}

void TvmOrchestrator::publish_etat() {
    nlohmann::json etat = {
        {"tvm_id", m_tvm_id},
        {"etat", state_to_string(m_supervisor.get_state())},
        {"timestamp", now_timestamp()}
    };
    m_state_publisher->publish("tvm/" + m_tvm_id + "/etat", etat.dump(), 1, true);
}

void TvmOrchestrator::publish_caisse() {
    nlohmann::json stock = nlohmann::json::object();
    double total = 0.0;
    for (const auto &entry : m_cash_drawer.get_cash()) {
        stock[std::to_string(entry.first)] = entry.second;
        total += (static_cast<double>(entry.first) / 100.0) * entry.second;
    }

    nlohmann::json caisse = {
        {"tvm_id", m_tvm_id},
        {"stock", stock},
        {"total", total},
        {"timestamp", now_timestamp()}
    };
    m_state_publisher->publish("tvm/" + m_tvm_id + "/caisse", caisse.dump(), 1, true);
}

void TvmOrchestrator::publish_vente(const std::string &mode_paiement, const std::string &compte_id,
                                     const std::string &transaction_id, double monnaie_rendue) {
    nlohmann::json vente = {
        {"tvm_id", m_tvm_id},
        {"transaction_id", transaction_id},
        {"type_titre", m_selected_type},
        {"prix_total", static_cast<double>(m_selected_price) / 100.0},
        {"mode_paiement", mode_paiement},
        {"monnaie_rendue", monnaie_rendue},
        {"timestamp", now_timestamp()}
    };
    if (!compte_id.empty())
        vente["compte_id"] = compte_id;

    m_state_publisher->publish("tvm/" + m_tvm_id + "/vente", vente.dump(), 2, false);
}

void TvmOrchestrator::publish_erreur(const std::string &type, const std::string &message) {
    nlohmann::json erreur = {
        {"tvm_id", m_tvm_id},
        {"type", type},
        {"message", message},
        {"timestamp", now_timestamp()}
    };
    m_state_publisher->publish("tvm/" + m_tvm_id + "/erreurs/" + type, erreur.dump(), 1, false);
}

void TvmOrchestrator::publish_annulation(double montant_rendu) {
    nlohmann::json annulation = {
        {"tvm_id", m_tvm_id},
        {"montant_rendu", montant_rendu},
        {"timestamp", now_timestamp()}
    };
    m_state_publisher->publish("tvm/" + m_tvm_id + "/annulation", annulation.dump(), 1, false);
}
