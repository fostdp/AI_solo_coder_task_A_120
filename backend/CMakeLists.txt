cmake_minimum_required(VERSION 3.15)
project(crossbow_sim_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/dynamics_model.cpp
    src/clickhouse_storage.cpp
    src/udp_receiver.cpp
    src/mqtt_alert_manager.cpp
    src/accuracy_analyzer.cpp
    src/http_server.cpp
)

set(HEADERS
    include/common.h
    include/dynamics_model.h
    include/clickhouse_storage.h
    include/udp_receiver.h
    include/mqtt_alert_manager.h
    include/accuracy_analyzer.h
    include/http_server.h
)

add_executable(crossbow_backend ${SOURCES} ${HEADERS})

target_include_directories(crossbow_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(crossbow_backend PRIVATE Ws2_32)
endif()

target_link_libraries(crossbow_backend PRIVATE Threads::Threads)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(UUID uuid)
    if(UUID_FOUND)
        target_include_directories(crossbow_backend PRIVATE ${UUID_INCLUDE_DIRS})
        target_link_libraries(crossbow_backend PRIVATE ${UUID_LIBRARIES})
    endif()
endif()

find_package(nlohmann_json 3.2.0 QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(crossbow_backend PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(crossbow_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/json/include)
endif()
