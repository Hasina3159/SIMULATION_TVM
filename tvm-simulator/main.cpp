#include "TvmOrchestrator.hpp"
#include "HeartbeatPublisher.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    const std::string server = "tcp://localhost:1883";
    const std::string tvm_id = "tvm_01";

    try {
        TvmOrchestrator orchestrator(server, tvm_id);
        HeartbeatPublisher heartbeat(server, tvm_id + "_hb", "tvm/" + tvm_id + "/heartbeat");

        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception &e) {
        std::cerr << "tvm-simulator: erreur fatale au demarrage : " << e.what() << std::endl;
        return (1);
    }
    return (0);
}
