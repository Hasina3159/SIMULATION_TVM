#pragma once
#include <string>
#include <random>

class IPrinterUnit {
public:
    virtual bool print(const std::string &data) = 0;
    virtual ~IPrinterUnit() = default;
};


class PrinterUnit : public IPrinterUnit {
private:
    std::mt19937 m_gen{std::random_device{}()};
public:
    PrinterUnit() = default;
    bool print(const std::string &data) override;
    ~PrinterUnit() = default;
};

