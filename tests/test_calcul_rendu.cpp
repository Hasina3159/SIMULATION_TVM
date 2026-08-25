#include <gtest/gtest.h>
#include "CashDrawer.hpp"

TEST(CalculRenduTest, RenduExactAvecGrossesCoupuresDabord) {
    std::map<unsigned long long, unsigned int> stock = {{500, 2}, {200, 3}, {100, 5}, {50, 10}};
    auto rendu = calculer_rendu(750, stock);

    ASSERT_TRUE(rendu.has_value());
    EXPECT_EQ((*rendu)[500], 1u);
    EXPECT_EQ((*rendu)[200], 1u);
    EXPECT_EQ((*rendu)[50], 1u);
}

TEST(CalculRenduTest, EchecExpliciteSiStockInsuffisant) {
    std::map<unsigned long long, unsigned int> stock = {{5000, 1}};  // pas de petites coupures
    auto rendu = calculer_rendu(300, stock);

    EXPECT_FALSE(rendu.has_value());
}

TEST(CalculRenduTest, MontantNulNeRendRien) {
    std::map<unsigned long long, unsigned int> stock = {{500, 2}, {200, 3}};
    auto rendu = calculer_rendu(0, stock);

    ASSERT_TRUE(rendu.has_value());
    EXPECT_TRUE(rendu->empty());
}

TEST(CalculRenduTest, UtiliseExactementLaCoupureQuandMontantEgal) {
    std::map<unsigned long long, unsigned int> stock = {{500, 1}};
    auto rendu = calculer_rendu(500, stock);

    ASSERT_TRUE(rendu.has_value());
    EXPECT_EQ((*rendu)[500], 1u);
}

TEST(CalculRenduTest, PasseAuxPlusPetitesCoupuresQuandLaGrosseEstEpuisee) {
    std::map<unsigned long long, unsigned int> stock = {{500, 1}, {200, 1}, {100, 1}};
    // 700 = 500 + 200, mais on ne veut qu'un seul billet de 500 dispo -> ok direct.
    // Testons plutot 900 avec un seul billet de 500 : 900 - 500 = 400 = 200+100+100(x manquant)
    stock = {{500, 1}, {200, 1}, {100, 2}};
    auto rendu = calculer_rendu(900, stock);

    ASSERT_TRUE(rendu.has_value());
    EXPECT_EQ((*rendu)[500], 1u);
    EXPECT_EQ((*rendu)[200], 1u);
    EXPECT_EQ((*rendu)[100], 2u);
}

TEST(CalculRenduTest, NeModifiePasLeStockRecu) {
    const std::map<unsigned long long, unsigned int> stock = {{500, 2}, {100, 5}};
    auto rendu = calculer_rendu(600, stock);

    ASSERT_TRUE(rendu.has_value());
    // Le stock passe en const& : verifie juste qu'on peut toujours le relire
    // avec les memes valeurs (aucune mutation cachee).
    EXPECT_EQ(stock.at(500), 2u);
    EXPECT_EQ(stock.at(100), 5u);
}
