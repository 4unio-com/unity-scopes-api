configure_file(Registry.ini.in Registry.ini)
configure_file(ScoperunnerRelativePath.ini.in ScoperunnerRelativePath.ini)

add_definitions(-DTEST_REGISTRY_PATH="${CMAKE_CURRENT_BINARY_DIR}/Registry.ini")

add_executable(RegistryConfig_test RegistryConfig_test.cpp)
target_link_libraries(RegistryConfig_test ${TESTLIBS})

add_test(RegistryConfig RegistryConfig_test)
