#pragma once
#include <sqlite3.h>
#include <string>
#include <mutex>

struct DebitResult {
    std::string statut;        // "accepte" | "refuse_solde_insuffisant" | "refuse_compte_inconnu"
    double nouveau_solde;
};

class ComptesManager {
private:
    sqlite3 *m_db;
    mutable std::mutex m_mutex;

    void init_schema();
    void seed_demo_account();

public:
    ComptesManager() = delete;
    ComptesManager(const ComptesManager &other) = delete;
    ComptesManager(ComptesManager &&other) = delete;
    ComptesManager &operator=(const ComptesManager &other) = delete;
    ComptesManager &operator=(ComptesManager &&other) = delete;

    explicit ComptesManager(const std::string &p_db_path);
    ~ComptesManager();

    DebitResult debiter(const std::string &p_compte_id, double p_montant, const std::string &p_transaction_id);
    double get_solde(const std::string &p_compte_id) const;
};
