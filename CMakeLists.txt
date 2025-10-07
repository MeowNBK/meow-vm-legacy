cmake_minimum_required(VERSION 3.25)
project(meow-vm LANGUAGES CXX)

# --- C++ Standard Configuration ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Cấu hình file meow-root ---
set(MEOW_ROOT_CONTENT "$ORIGIN/..")
set(MEOW_ROOT_FILE "${CMAKE_BINARY_DIR}/bin/meow-root")
file(WRITE ${MEOW_ROOT_FILE} "${MEOW_ROOT_CONTENT}\n")

# --- Main Executable: meow-vm ---
file(GLOB_RECURSE VM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${VM_SOURCES})

# Đặt vị trí output cho file thực thi chính
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

# Các đường dẫn include cho target chính
target_include_directories(${PROJECT_NAME} PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/common"
    "${PROJECT_SOURCE_DIR}/include/runtime"
    "${PROJECT_SOURCE_DIR}/include/vm"
    "${PROJECT_SOURCE_DIR}/include/memory"
    "${PROJECT_SOURCE_DIR}/include/loader"
    "${PROJECT_SOURCE_DIR}/include/module"
)

# --- Precompiled Headers (PCH) ---
set(PCH_HEADER "${PROJECT_SOURCE_DIR}/include/common/pch.h")
if (EXISTS "${PCH_HEADER}")
    message(STATUS "PCH: Found ${PCH_HEADER} -> Enabling precompiled headers.")
    target_precompile_headers(${PROJECT_NAME} PRIVATE "${PCH_HEADER}")
else()
    message(STATUS "PCH: ${PCH_HEADER} not found -> Precompiled headers disabled.")
endif()


# --- Standard Library Modules (Shared Libraries) ---
file(GLOB STD_SOURCES CONFIGURE_DEPENDS "stdlib/src/*.cpp")

foreach(STD_FILE ${STD_SOURCES})
    get_filename_component(MOD_NAME ${STD_FILE} NAME_WE)
    
    add_library(${MOD_NAME} SHARED ${STD_FILE})

    target_include_directories(${MOD_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/stdlib/common"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include/common"
        "${PROJECT_SOURCE_DIR}/include/runtime"
        "${PROJECT_SOURCE_DIR}/include/vm"
        "${PROJECT_SOURCE_DIR}/include/memory"
        "${PROJECT_SOURCE_DIR}/include/loader"
        "${PROJECT_SOURCE_DIR}/include/module"
    )
    
    target_compile_definitions(${MOD_NAME} PRIVATE MEOW_STDLIB_EXPORTS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MOD_NAME})
    
    if (EXISTS "${PCH_HEADER}")
        target_precompile_headers(${MOD_NAME} PRIVATE "${PCH_HEADER}")
    endif()

    set_target_properties(${MOD_NAME} PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/stdlib"
    )
endforeach()
