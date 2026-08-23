#include "TvmSupervisor.hpp"


TvmSupervisor::TvmSupervisor() : 
    m_state(TvmState::IDLE)
{
        m_order[TvmState::IDLE] = {TvmState::SELECTING_TICKET};
        m_order[TvmState::SELECTING_TICKET] = {TvmState::SUMMARIZING_ORDER};
        m_order[TvmState::SUMMARIZING_ORDER] = {TvmState::AWAITING_PAYMENT};
        m_order[TvmState::AWAITING_PAYMENT] = {TvmState::VALIDATING_PAYMENT};
        m_order[TvmState::VALIDATING_PAYMENT] = {TvmState::PRINTING, TvmState::ERROR};
        m_order[TvmState::PRINTING] = {TvmState::DISPENSING};
        m_order[TvmState::ERROR] = {TvmState::IDLE};
        m_order[TvmState::DISPENSING] = {TvmState::IDLE};
}

bool TvmSupervisor::set_state(TvmState p_state)
{
    for (auto &state : m_order[m_state]) {
        if (p_state == state) {
            m_state = p_state;
            return (true);
        }
    }
    return (false);
}

TvmState TvmSupervisor::get_state() const
{
    return (m_state);
}
