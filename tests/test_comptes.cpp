#include <gtest/gtest.h>
#include "ComptesManager.hpp"
#include <thread>
#include <vector>
#include <atomic>

TEST(ComptesManagerTest, SeedDuCompteDemoAuDemarrage) {
    ComptesManager comptes(":memory:");
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_demo"), 50.0);
}

TEST(ComptesManagerTest, DebitAccepteSiSoldeSuffisant) {
    ComptesManager comptes(":memory:");

    DebitResult result = comptes.debiter("cl_demo", 14.50, "tx_001");

    EXPECT_EQ(result.statut, "accepte");
    EXPECT_DOUBLE_EQ(result.nouveau_solde, 35.50);
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_demo"), 35.50);
}

TEST(ComptesManagerTest, DebitRefuseSiSoldeInsuffisantEtSoldeInchange) {
    ComptesManager comptes(":memory:");

    DebitResult result = comptes.debiter("cl_demo", 100.0, "tx_002");

    EXPECT_EQ(result.statut, "refuse_solde_insuffisant");
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_demo"), 50.0);
}

TEST(ComptesManagerTest, DebitRefuseSiCompteInconnu) {
    ComptesManager comptes(":memory:");

    DebitResult result = comptes.debiter("cl_inexistant", 5.0, "tx_003");

    EXPECT_EQ(result.statut, "refuse_compte_inconnu");
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_inexistant"), 0.0);
}

TEST(ComptesManagerTest, GetSoldeCompteInconnuRenvoieZero) {
    ComptesManager comptes(":memory:");
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_jamais_vu"), 0.0);
}

TEST(ComptesManagerTest, DeuxDebitsConcurrentsNeDoublentJamaisLeDebit) {
    // Solde initial 50€. Deux threads tentent chacun de debiter 30€ en meme
    // temps : un seul doit reussir (BEGIN IMMEDIATE empeche la lecture d'un
    // solde perime par le second thread).
    ComptesManager comptes(":memory:");

    std::atomic<int> acceptes{0};
    auto tentative = [&comptes, &acceptes]() {
        DebitResult r = comptes.debiter("cl_demo", 30.0, "tx_concurrent");
        if (r.statut == "accepte")
            acceptes++;
    };

    std::thread t1(tentative), t2(tentative);
    t1.join();
    t2.join();

    EXPECT_EQ(acceptes.load(), 1);
    EXPECT_DOUBLE_EQ(comptes.get_solde("cl_demo"), 20.0);
}
