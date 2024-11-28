cmake_minimum_required(VERSION 3.30)
project(delphioracle)

find_package(cdt)

add_contract(delphioracle delphioracle delphioracle.cpp)
target_include_directories(delphioracle PUBLIC ${CMAKE_SOURCE_DIR}/../include)
target_ricardian_directory(delphioracle ${CMAKE_SOURCE_DIR}/../ricardian)
