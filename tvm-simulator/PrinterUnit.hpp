#pragma once
#include <string>

class IPrinterUnit {
public:
    virtual bool print(const std::string &data) = 0;
    virtual ~IPrinterUnit() = default;
};


class PrinterUnit : public IPrinterUnit {

public:
    PrinterUnit() = default;
    bool print(const std::string &data) override;
    ~PrinterUnit() = default;
};

