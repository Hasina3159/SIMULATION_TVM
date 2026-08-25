cmake_minimum_required(VERSION 3.16)

project(TvmSimulator VERSION 1.0.0 LANGUAGES CXX)

set(COMMON_FLAGS -Wall -Wextra -Werror)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(PahoMqttCpp REQUIRED)
find_package(SQLite3 REQUIRED)


add_executable(collector
    collector/main.cpp
    collector/ComptesManager.cpp
    collector/MqttListener.cpp
    tvm-simulator/MqttPublisher.cpp
    tvm-simulator/MqttSubscriber.cpp
)

add_executable(tvm-simulator
    tvm-simulator/main.cpp
    tvm-simulator/TvmSupervisor.cpp
    tvm-simulator/CashDrawer.cpp
    tvm-simulator/PrinterUnit.cpp
    tvm-simulator/HeartbeatPublisher.cpp
    tvm-simulator/MqttSubscriber.cpp
    tvm-simulator/MqttPublisher.cpp
    tvm-simulator/CardReader.cpp
    tvm-simulator/TvmOrchestrator.cpp
)


#========== TEST ===============
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(test_supervisor
    tests/test_supervisor.cpp
    tests/test_calcul_rendu.cpp
    tvm-simulator/TvmSupervisor.cpp
    tvm-simulator/CashDrawer.cpp
)

target_link_libraries(test_supervisor PRIVATE GTest::gtest_main)
target_include_directories(test_supervisor PRIVATE tvm-simulator)

add_executable(test_comptes
    tests/test_comptes.cpp
    collector/ComptesManager.cpp
)
target_link_libraries(test_comptes PRIVATE GTest::gtest_main Threads::Threads SQLite::SQLite3)
target_include_directories(test_comptes PRIVATE collector)

add_executable(test_card_reader
    tests/test_card_reader.cpp
    tvm-simulator/CardReader.cpp
    tvm-simulator/MqttPublisher.cpp
    tvm-simulator/MqttSubscriber.cpp
)
target_link_libraries(test_card_reader PRIVATE GTest::gtest_main Threads::Threads PahoMqttCpp::paho-mqttpp3)
target_include_directories(test_card_reader PRIVATE tvm-simulator third_party)

include(GoogleTest)
gtest_discover_tests(test_supervisor)
gtest_discover_tests(test_card_reader)
gtest_discover_tests(test_comptes)
#========== TEST ===============


target_include_directories(collector PRIVATE
    collector
    tvm-simulator
    common
    third_party
)
target_include_directories(tvm-simulator PRIVATE tvm-simulator common third_party)

target_compile_options(collector PRIVATE ${COMMON_FLAGS})
target_compile_options(tvm-simulator PRIVATE ${COMMON_FLAGS})

target_link_libraries(collector PRIVATE Threads::Threads SQLite::SQLite3 PahoMqttCpp::paho-mqttpp3)
target_link_libraries(tvm-simulator PRIVATE Threads::Threads PahoMqttCpp::paho-mqttpp3)