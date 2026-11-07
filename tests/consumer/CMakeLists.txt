cmake_minimum_required(VERSION 3.20)
project(apfloat_consumer LANGUAGES CXX)
include(CTest)

set(APFLOAT_SOURCE_DIR "" CACHE PATH "Use an apfloat source tree instead of an installed package")
if(APFLOAT_SOURCE_DIR)
  add_subdirectory("${APFLOAT_SOURCE_DIR}" apfloat EXCLUDE_FROM_ALL)
else()
  find_package(apfloat CONFIG REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE apfloat::apfloat)
