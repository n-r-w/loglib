set(name loglib-demo)

project(${name} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${name}
   main.cpp
)

target_link_libraries(${name}
    loglib
)

target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../loglib
    ../loglib/3rdparty
)

target_compile_definitions(${name} PRIVATE
    FMT_HEADER_ONLY
)
