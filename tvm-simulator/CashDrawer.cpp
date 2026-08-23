#include "CashDrawer.hpp"
#include <stdexcept>
#include <string>
#include <algorithm>

std::optional<std::map<unsigned long long, unsigned int>> calculer_rendu(
    unsigned long long p_amount,
    const std::map<unsigned long long, unsigned int> &p_stock)
{
    std::map<unsigned long long, unsigned int> rendu;

    for (auto it = p_stock.crbegin(); it != p_stock.crend(); it++) {
        unsigned long long value = it->first;
        unsigned int available = it->second;

        if (value <= p_amount) {
            unsigned int unit = std::min<unsigned long long>(available, p_amount / value);
            p_amount -= value * unit;
            if (unit > 0)
                rendu[value] = unit;
        }
    }

    if (p_amount == 0)
        return rendu;
    return std::nullopt;
}

CashDrawer::CashDrawer()
{
    m_cash[10] = 0;
    m_cash[20] = 0;
    m_cash[50] = 0;
    m_cash[100] = 0;
    m_cash[200] = 0;
    m_cash[500] = 0;
    m_cash[1000] = 0;
    m_cash[2000] = 0;
    m_cash[5000] = 0;

    for (const auto &entry : m_cash)
        m_tmp_cash[entry.first] = 0;
}

CashDrawer::CashDrawer(const std::map<unsigned long long, unsigned int> &p_cash) : CashDrawer()
{
    for (auto &extern_cash : p_cash) {
        if (m_cash.count(extern_cash.first) == 0) {
            throw std::runtime_error(std::string("Invalid Cash value : ") + std::to_string(extern_cash.first));
        }
        m_cash[extern_cash.first] = extern_cash.second;
    }
}

CashDrawer::CashDrawer(CashDrawer &&other) noexcept
{
    *this = std::move(other);
}

CashDrawer::CashDrawer(const CashDrawer &other)
{
    *this = other;
}

CashDrawer &CashDrawer::operator=(const CashDrawer &other)
{
    if (this != &other) {
        this->m_cash = other.m_cash;
        this->m_tmp_cash = other.m_tmp_cash;
    }
    return (*this);
}

CashDrawer &CashDrawer::operator=(CashDrawer &&other)
{
    if (this != &other) {
        this->m_cash = std::move(other.m_cash);
        this->m_tmp_cash = std::move(other.m_tmp_cash);
    }
    return (*this);
}

CashDrawer::~CashDrawer()
{
    m_cash.clear();
    m_tmp_cash.clear();
}

CashDrawerErrorStatus CashDrawer::provision(Denomination &p_cash)
{
    if (m_cash.count(p_cash.value) == 0)
        return (CashDrawerErrorStatus::INVALID_CASH);
    m_cash[p_cash.value] = m_cash[p_cash.value] + p_cash.quantity;
    return (CashDrawerErrorStatus::OK);
}

CashDrawerErrorStatus CashDrawer::add_cash(Denomination &p_cash)
{
    if (m_cash.count(p_cash.value) == 0)
        return (CashDrawerErrorStatus::INVALID_CASH);
    m_tmp_cash[p_cash.value] = m_tmp_cash[p_cash.value] + p_cash.quantity;
    return (CashDrawerErrorStatus::OK);
}

std::map<unsigned long long, unsigned int> CashDrawer::retire_cash(unsigned long long p_amount, CashDrawerErrorStatus &error_status)
{
    std::map<unsigned long long, unsigned int> available = m_cash;
    for (const auto &entry : m_tmp_cash)
        available[entry.first] += entry.second;

    auto rendu = calculer_rendu(p_amount, available);
    if (!rendu.has_value()) {
        error_status = CashDrawerErrorStatus::NOT_ENOUGH_CASH;
        return (std::map<unsigned long long, unsigned int>());
    }

    for (auto &entry : m_tmp_cash) {
        m_cash[entry.first] += entry.second;
        entry.second = 0;
    }
    for (const auto &entry : *rendu)
        m_cash[entry.first] -= entry.second;

    error_status = CashDrawerErrorStatus::OK;
    return (*rendu);
}

std::map<unsigned long long, unsigned int> CashDrawer::rollback()
{
    std::map<unsigned long long, unsigned int> returned = m_tmp_cash;
    for (auto &entry : m_tmp_cash)
        entry.second = 0;
    return (returned);
}

const std::map<unsigned long long, unsigned int> &CashDrawer::get_cash() const
{
    return (m_cash);
}

const std::map<unsigned long long, unsigned int> &CashDrawer::get_pending_cash() const
{
    return (m_tmp_cash);
}

// ================================================================================================

CashDrawerBuilder::CashDrawerBuilder() : m_cashdrawer() {
}

CashDrawerBuilder &CashDrawerBuilder::add_cash(Denomination &p_cash) {
    m_cashdrawer.provision(p_cash);
    return (*this);
}

CashDrawerBuilder &CashDrawerBuilder::add_cash(Denomination &&p_cash) {
    m_cashdrawer.provision(p_cash);
    return (*this);
}

CashDrawer CashDrawerBuilder::terminate() {
    return (std::move(m_cashdrawer));
}
