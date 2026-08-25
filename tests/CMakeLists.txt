add_executable(backtest_tests
  test_types.cpp
  test_data_loader.cpp
  test_account.cpp
  test_exchange.cpp
  test_strategy.cpp
  test_engine.cpp
  test_metrics.cpp
)
target_link_libraries(backtest_tests PRIVATE backtest_core GTest::gtest_main)
add_test(NAME backtest_tests COMMAND backtest_tests)
