#include "TvmSupervisor.hpp"
#include "CashDrawer.hpp"
#include "PrinterUnit.hpp"
#include <iostream>
#include <memory>

int main() {
    CashDrawer cashDrawer = CashDrawerBuilder()
        .add_cash({10, 20})
        .add_cash({20, 20})
        .add_cash({50, 20})
        .add_cash({100, 20})
        .add_cash({200, 20})
        .add_cash({500, 20})
        .add_cash({1000, 20})
        .add_cash({2000, 20})
        .add_cash({5000, 20})
        .terminate();
    
    CashDrawerErrorStatus error_status;
    std::map <unsigned long long, unsigned int> retired = cashDrawer.retire_cash(39670, error_status);

    for (auto &element : retired) {
        std::cout << element.first << " : " << element.second << std::endl;
    }

    if (error_status == CashDrawerErrorStatus::OK)
        std::cout << "OK" << std::endl;
    else 
        std::cout << "KO" << std::endl;

    std::unique_ptr<IPrinterUnit> printer = std::make_unique<PrinterUnit>();
    int nb_ko = 0;
    for (int i = 0; i < 10000; i++) {
        if (printer->print("Anana") == false)
            nb_ko++;
    }
    std::cout << "Pourcentage : " << (nb_ko / 10000.0) * 100.0  << "%" << std::endl;
    
    return (0);
}