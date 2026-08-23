#pragma once
#include <map>
#include <optional>

enum class CashDrawerErrorStatus {
    OK,
    INVALID_CASH,
    INVALID_AMOUNT,
    NOT_ENOUGH_CASH
};

struct Denomination {
    unsigned long long value;
    unsigned int quantity;
};

std::optional<std::map<unsigned long long, unsigned int>> calculer_rendu(
    unsigned long long p_amount,
    const std::map<unsigned long long, unsigned int> &p_stock);

class ICashDrawer {
protected:
    std::map <unsigned long long, unsigned int> m_cash;
    std::map <unsigned long long, unsigned int> m_tmp_cash;
public:
    virtual CashDrawerErrorStatus provision(Denomination &p_cash) = 0;
    virtual CashDrawerErrorStatus add_cash(Denomination &p_cash) = 0;
    virtual std::map <unsigned long long, unsigned int> retire_cash(unsigned long long p_amount, CashDrawerErrorStatus &error_status) = 0;
    virtual std::map <unsigned long long, unsigned int> rollback() = 0;
    virtual const std::map <unsigned long long, unsigned int> &get_cash() const = 0;
    virtual const std::map <unsigned long long, unsigned int> &get_pending_cash() const = 0;
    virtual ~ICashDrawer() = default;
};

class CashDrawer : public ICashDrawer
{
public:
    CashDrawer();
    CashDrawer(const std::map <unsigned long long, unsigned int> &p_cash);
    CashDrawer(CashDrawer &&other) noexcept;
    CashDrawer(const CashDrawer &other);
    CashDrawer &operator=(const CashDrawer &other);
    CashDrawer &operator=(CashDrawer &&other);
    ~CashDrawer();

    CashDrawerErrorStatus provision(Denomination &p_cash) override;
    CashDrawerErrorStatus add_cash(Denomination &p_cash) override;
    std::map <unsigned long long, unsigned int> retire_cash(unsigned long long p_amount, CashDrawerErrorStatus &error_status) override;
    std::map <unsigned long long, unsigned int> rollback() override;

    const std::map <unsigned long long, unsigned int> &get_cash() const override;
    const std::map <unsigned long long, unsigned int> &get_pending_cash() const override;
};

class CashDrawerBuilder {
private:
    CashDrawer m_cashdrawer;

public:
    CashDrawerBuilder();
    ~CashDrawerBuilder() = default;
    CashDrawerBuilder &add_cash(Denomination &p_cash);
    CashDrawerBuilder &add_cash(Denomination &&p_cash);
    CashDrawer terminate();
};
