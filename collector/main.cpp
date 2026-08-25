#include "ComptesManager.hpp"
#include "MqttListener.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        ComptesManager comptes("collector.db");
        std::cout << "Collector demarre. Compte demo 'cl_demo' : "
                  << comptes.get_solde("cl_demo") << " EUR" << std::endl;

        MqttListener listener("tcp://localhost:1883", "collector", comptes);
        std::cout << "En ecoute sur tvm/+/paiement_compte/demande ... (Ctrl+C pour arreter)" << std::endl;

        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception &e) {
        std::cerr << "Collector: erreur fatale au demarrage : " << e.what() << std::endl;
        return (1);
    }
    return (0);
}
