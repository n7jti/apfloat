cmake_minimum_required(VERSION 3.20)
project(apfloat VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(apfloat STATIC src/apfloat.cpp)
add_library(apfloat::apfloat ALIAS apfloat)
target_compile_features(apfloat PUBLIC cxx_std_17)
target_include_directories(apfloat PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(TARGETS apfloat EXPORT apfloatTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME})
install(EXPORT apfloatTargets NAMESPACE apfloat:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apfloat)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/apfloatConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/apfloatConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apfloat)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/apfloatConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/apfloatConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/apfloatConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apfloat)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  include(CTest)
  if(BUILD_TESTING)
    add_executable(apfloat_unit tests/unit.cpp)
    target_link_libraries(apfloat_unit PRIVATE apfloat::apfloat)
    add_test(NAME unit COMMAND apfloat_unit)
    set_tests_properties(unit PROPERTIES LABELS unit)
    add_executable(apfloat_functional tests/functional.cpp)
    target_link_libraries(apfloat_functional PRIVATE apfloat::apfloat)
    add_test(NAME functional COMMAND apfloat_functional)
    set_tests_properties(functional PROPERTIES LABELS functional)
    add_test(NAME uncaught_mismatch COMMAND ${CMAKE_COMMAND}
      "-DPROGRAM=$<TARGET_FILE:apfloat_functional>"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/uncaught.cmake)
    set_tests_properties(uncaught_mismatch PROPERTIES LABELS functional)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_program(BASH_EXECUTABLE bash REQUIRED)
      find_program(SCRIPT_EXECUTABLE script REQUIRED)
      add_test(NAME pi_scripts COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pi-scripts.sh)
      set_tests_properties(pi_scripts PROPERTIES LABELS functional)
    endif()
    add_executable(apfloat_benchmark tests/benchmark.cpp)
    target_link_libraries(apfloat_benchmark PRIVATE apfloat::apfloat)
    target_compile_definitions(apfloat_benchmark PRIVATE
      APFLOAT_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
      APFLOAT_CONFIGURATION="$<CONFIG>")
    add_test(NAME benchmark COMMAND apfloat_benchmark "${CMAKE_CURRENT_BINARY_DIR}/benchmark.csv")
    set_tests_properties(benchmark PROPERTIES LABELS performance)
  endif()
endif()
