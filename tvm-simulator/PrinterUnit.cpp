#include "PrinterUnit.hpp"
#include <random>
#include <iostream>

bool PrinterUnit::print(const std::string &data)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> distrib(0, 100);
    if (distrib(gen) <= 2)
        return (false);
    std::cout << data << std::endl; 
    return (true);
}
