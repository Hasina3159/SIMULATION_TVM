#include <gtest/gtest.h>
#include "TvmSupervisor.hpp"

TEST(TvmSupervisorTest, InitialStateIsIdle) {
    TvmSupervisor supervisor;

    EXPECT_EQ(supervisor.get_state(), TvmState::IDLE);
}

TEST(TvmSupervisorTest, RejectsPrintingFromIdle) {
    TvmSupervisor supervisor;

    EXPECT_FALSE(supervisor.set_state(TvmState::PRINTING));
    EXPECT_EQ(supervisor.get_state(), TvmState::IDLE);
}

TEST(TvmSupervisorTest, RejectsErrorFromIdle) {
    TvmSupervisor supervisor;

    EXPECT_FALSE(supervisor.set_state(TvmState::ERROR));
    EXPECT_EQ(supervisor.get_state(), TvmState::IDLE);
}

TEST(TvmSupervisorTest, AcceptsSelectingTicketFromIdle) {
    TvmSupervisor supervisor;

    EXPECT_TRUE(supervisor.set_state(TvmState::SELECTING_TICKET));
    EXPECT_EQ(supervisor.get_state(), TvmState::SELECTING_TICKET);
}

TEST(TvmSupervisorTest, RejectsSkippingAheadFromSelectingTicket) {
    TvmSupervisor supervisor;
    supervisor.set_state(TvmState::SELECTING_TICKET);

    EXPECT_FALSE(supervisor.set_state(TvmState::PRINTING));
    EXPECT_EQ(supervisor.get_state(), TvmState::SELECTING_TICKET);
}

TEST(TvmSupervisorTest, FollowsFullHappyPathBackToIdle) {
    TvmSupervisor supervisor;

    EXPECT_TRUE(supervisor.set_state(TvmState::SELECTING_TICKET));
    EXPECT_TRUE(supervisor.set_state(TvmState::SUMMARIZING_ORDER));
    EXPECT_TRUE(supervisor.set_state(TvmState::AWAITING_PAYMENT));
    EXPECT_TRUE(supervisor.set_state(TvmState::VALIDATING_PAYMENT));
    EXPECT_TRUE(supervisor.set_state(TvmState::PRINTING));
    EXPECT_TRUE(supervisor.set_state(TvmState::DISPENSING));
    EXPECT_TRUE(supervisor.set_state(TvmState::IDLE));

    EXPECT_EQ(supervisor.get_state(), TvmState::IDLE);
}

TEST(TvmSupervisorTest, AcceptsErrorFromValidatingPayment) {
    TvmSupervisor supervisor;
    supervisor.set_state(TvmState::SELECTING_TICKET);
    supervisor.set_state(TvmState::SUMMARIZING_ORDER);
    supervisor.set_state(TvmState::AWAITING_PAYMENT);
    supervisor.set_state(TvmState::VALIDATING_PAYMENT);

    EXPECT_TRUE(supervisor.set_state(TvmState::ERROR));
    EXPECT_EQ(supervisor.get_state(), TvmState::ERROR);
}

TEST(TvmSupervisorTest, RecoversToIdleAfterError) {
    TvmSupervisor supervisor;
    supervisor.set_state(TvmState::SELECTING_TICKET);
    supervisor.set_state(TvmState::SUMMARIZING_ORDER);
    supervisor.set_state(TvmState::AWAITING_PAYMENT);
    supervisor.set_state(TvmState::VALIDATING_PAYMENT);
    supervisor.set_state(TvmState::ERROR);

    EXPECT_TRUE(supervisor.set_state(TvmState::IDLE));
    EXPECT_EQ(supervisor.get_state(), TvmState::IDLE);
}
