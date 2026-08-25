#include "ComptesManager.hpp"
#include <stdexcept>
#include <chrono>

namespace {

constexpr const char *DEMO_COMPTE_ID = "cl_demo";
constexpr double DEMO_SOLDE_INITIAL = 50.0;

long long now_timestamp() {
    return (std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace

ComptesManager::ComptesManager(const std::string &p_db_path) : m_db(nullptr)
{
    if (sqlite3_open(p_db_path.c_str(), &m_db) != SQLITE_OK) {
        std::string message = m_db ? sqlite3_errmsg(m_db) : "sqlite3_open a echoue";
        if (m_db)
            sqlite3_close(m_db);
        throw std::runtime_error("ComptesManager: impossible d'ouvrir la base (" + p_db_path + ") : " + message);
    }
    init_schema();
    seed_demo_account();
}

ComptesManager::~ComptesManager()
{
    if (m_db)
        sqlite3_close(m_db);
}

void ComptesManager::init_schema()
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS comptes ("
        "    compte_id TEXT PRIMARY KEY,"
        "    solde REAL NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS mouvements_solde ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    compte_id TEXT NOT NULL,"
        "    montant REAL NOT NULL,"
        "    solde_apres REAL NOT NULL,"
        "    transaction_id TEXT,"
        "    timestamp INTEGER NOT NULL"
        ");";

    char *error_message = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::string message = error_message ? error_message : "erreur inconnue";
        sqlite3_free(error_message);
        throw std::runtime_error("ComptesManager: creation du schema echouee (" + message + ")");
    }
}

void ComptesManager::seed_demo_account()
{
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "INSERT OR IGNORE INTO comptes(compte_id, solde) VALUES (?, ?);";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("ComptesManager: preparation du seed echouee");

    sqlite3_bind_text(stmt, 1, DEMO_COMPTE_ID, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, DEMO_SOLDE_INITIAL);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

DebitResult ComptesManager::debiter(const std::string &p_compte_id, double p_montant, const std::string &p_transaction_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

    sqlite3_stmt *select_stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT solde FROM comptes WHERE compte_id = ?;", -1, &select_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_compte_inconnu", 0.0});
    }
    sqlite3_bind_text(select_stmt, 1, p_compte_id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(select_stmt) != SQLITE_ROW) {
        sqlite3_finalize(select_stmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_compte_inconnu", 0.0});
    }

    double solde_actuel = sqlite3_column_double(select_stmt, 0);
    sqlite3_finalize(select_stmt);

    if (solde_actuel < p_montant) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_solde_insuffisant", solde_actuel});
    }

    double nouveau_solde = solde_actuel - p_montant;

    sqlite3_stmt *update_stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "UPDATE comptes SET solde = ? WHERE compte_id = ?;", -1, &update_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_compte_inconnu", solde_actuel});
    }
    sqlite3_bind_double(update_stmt, 1, nouveau_solde);
    sqlite3_bind_text(update_stmt, 2, p_compte_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(update_stmt) != SQLITE_DONE) {
        sqlite3_finalize(update_stmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_compte_inconnu", solde_actuel});
    }
    sqlite3_finalize(update_stmt);

    sqlite3_stmt *insert_stmt = nullptr;
    const char *insert_sql =
        "INSERT INTO mouvements_solde(compte_id, montant, solde_apres, transaction_id, timestamp) "
        "VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(m_db, insert_sql, -1, &insert_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return (DebitResult{"refuse_compte_inconnu", solde_actuel});
    }
    sqlite3_bind_text(insert_stmt, 1, p_compte_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(insert_stmt, 2, -p_montant);
    sqlite3_bind_double(insert_stmt, 3, nouveau_solde);
    sqlite3_bind_text(insert_stmt, 4, p_transaction_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insert_stmt, 5, now_timestamp());
    sqlite3_step(insert_stmt);
    sqlite3_finalize(insert_stmt);

    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);

    return (DebitResult{"accepte", nouveau_solde});
}

double ComptesManager::get_solde(const std::string &p_compte_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT solde FROM comptes WHERE compte_id = ?;", -1, &stmt, nullptr) != SQLITE_OK)
        return (0.0);
    sqlite3_bind_text(stmt, 1, p_compte_id.c_str(), -1, SQLITE_TRANSIENT);

    double solde = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        solde = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return (solde);
}
