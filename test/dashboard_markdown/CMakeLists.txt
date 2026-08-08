add_executable(DashboardMarkdownTest
  DashboardMarkdownTest.cpp
  ${REPO_ROOT}/src/util/DashboardMarkdown.cpp
)

target_include_directories(DashboardMarkdownTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(DashboardMarkdownTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DashboardMarkdownTest)
