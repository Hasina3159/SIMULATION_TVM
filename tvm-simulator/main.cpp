#include "TvmSupervisor.hpp"
#include "CashDrawer.hpp"
#include <iostream>

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

    I
    
    return (0);
}