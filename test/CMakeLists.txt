add_executable(sce_host_tests test_main.c)

target_link_libraries(sce_host_tests PRIVATE serial_command_extreme::serial_command_extreme)

target_compile_features(sce_host_tests PRIVATE c_std_99)

add_test(NAME sce_host_tests COMMAND sce_host_tests)
