#pragma once
#include <unordered_map>
#include <vector>

enum class TvmState {
  IDLE,
  SELECTING_TICKET,
  SUMMARIZING_ORDER,
  AWAITING_PAYMENT,
  VALIDATING_PAYMENT,
  PRINTING,
  DISPENSING,
  ERROR
};

class TvmSupervisor
{
private:
    std::unordered_map <TvmState, std::vector <TvmState> > m_order;
    TvmState m_state;
public:
    TvmSupervisor();
    TvmSupervisor(const TvmSupervisor &other) = delete;
    TvmSupervisor &operator=(const TvmSupervisor &other) = delete;
    ~TvmSupervisor() = default;

    bool set_state(TvmState p_state);
    TvmState get_state() const;
};
