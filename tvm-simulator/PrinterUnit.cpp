#include "PrinterUnit.hpp"
#include <iostream>

bool PrinterUnit::print(const std::string &data)
{
    std::uniform_int_distribution<int> distrib(0, 99);
    if (distrib(m_gen) < 2)
        return (false);
    std::cout << data << std::endl; 
    return (true);
}
