set(name loglib)

project(${name} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${name}
   manager.h
   manager.cpp
   worker.h
   worker.cpp
   record.h
   record.cpp
   stoppable_worker.h
   stoppable_worker.cpp
)

target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    3rdparty
)

target_compile_definitions(${name} PRIVATE
    FMT_HEADER_ONLY
    CPPHTTPLIB_NO_EXCEPTIONS    
)
