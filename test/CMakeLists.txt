add_executable(
  sce_host_tests
  test_main.c
  test_bsc_tokenizer.c
  test_bsc_types.c
  test_bsc_registry.c
  test_bsc_matcher.c
  test_bsc_args.c
  test_bsc_dispatch.c
)

target_link_libraries(sce_host_tests PRIVATE serial_command_extreme::serial_command_extreme)

target_compile_features(sce_host_tests PRIVATE c_std_99)

add_test(NAME sce_host_tests COMMAND sce_host_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(
    NAME sce_forbidden_patterns
    COMMAND
      ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tools/check_forbidden_patterns.py
      ${PROJECT_SOURCE_DIR}/src
  )
else()
  message(STATUS "Python3 interpreter not found; skipping sce_forbidden_patterns CTest registration.")
endif()
