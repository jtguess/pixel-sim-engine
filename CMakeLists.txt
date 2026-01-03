cmake_minimum_required(VERSION 3.20)
project(pixel_sim_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# Options
# ============================================
option(ENGINE_BUILD_SHADERS "Compile shaders at build time" ON)
option(ENGINE_AUTO_BUILD_SHADERC "Automatically build shaderc tool" OFF)
option(ENGINE_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" ON)

# Paths (can be overridden via -D flags)
set(ENGINE_SHADERC_PATH "" CACHE FILEPATH "Path to shaderc executable")
set(BGFX_SHADER_INCLUDE_DIR "" CACHE PATH "Path to bgfx shader includes (contains bgfx_shader.sh)")

# ============================================
# Find Dependencies
# ============================================

# SDL2 (via vcpkg)
find_package(SDL2 REQUIRED)

# stb (via vcpkg) - header-only library
find_path(STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)
if(NOT STB_INCLUDE_DIR)
    message(FATAL_ERROR "stb not found. Make sure it's installed via vcpkg.")
endif()
message(STATUS "Found stb: ${STB_INCLUDE_DIR}")

# bgfx (via bgfx.cmake submodule)
if(NOT TARGET bgfx::bgfx)
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/bgfx.cmake EXCLUDE_FROM_ALL)
endif()

# ============================================
# Source Files
# ============================================

set(ENGINE_SOURCES
    src/main.cpp
    src/TextureManager.cpp
    src/SpriteBatch.cpp
)

set(ENGINE_HEADERS
    include/TextureManager.h
    include/SpriteBatch.h
)

# ============================================
# Shader Compilation
# ============================================

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/bin")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# All shaders to compile
set(VERTEX_SHADERS
    shaders/vs_blit.sc
    shaders/vs_sprite.sc
)

set(FRAGMENT_SHADERS
    shaders/fs_blit.sc
    shaders/fs_sprite.sc
)

set(SHADER_VARYING_DEF "${CMAKE_SOURCE_DIR}/shaders/varying.def.sc")

# Platform-specific shader settings
if(APPLE)
    set(SHADER_PLATFORM "osx")
    set(SHADER_PROFILE_VS "metal")
    set(SHADER_PROFILE_FS "metal")
elseif(WIN32)
    set(SHADER_PLATFORM "windows")
    set(SHADER_PROFILE_VS "s_5_0")
    set(SHADER_PROFILE_FS "s_5_0")
else()
    set(SHADER_PLATFORM "linux")
    set(SHADER_PROFILE_VS "spirv")
    set(SHADER_PROFILE_FS "spirv")
endif()

# Function to compile a shader
function(compile_shader SHADER_FILE SHADER_TYPE SHADER_PROFILE)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.bin")
    
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${ENGINE_SHADERC_PATH}
            -f ${CMAKE_SOURCE_DIR}/${SHADER_FILE}
            -o ${OUTPUT_FILE}
            --type ${SHADER_TYPE}
            --platform ${SHADER_PLATFORM}
            --profile ${SHADER_PROFILE}
            -i ${BGFX_SHADER_INCLUDE_DIR}
            --varyingdef ${SHADER_VARYING_DEF}
        DEPENDS 
            ${CMAKE_SOURCE_DIR}/${SHADER_FILE}
            ${SHADER_VARYING_DEF}
        COMMENT "Compiling shader: ${SHADER_NAME}.sc -> ${SHADER_NAME}.bin"
        VERBATIM
    )
    
    # Return output file path to parent scope
    set(${SHADER_NAME}_OUTPUT ${OUTPUT_FILE} PARENT_SCOPE)
    list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} PARENT_SCOPE)
endfunction()

# Compile all shaders if enabled
set(SHADER_OUTPUTS "")

if(ENGINE_BUILD_SHADERS)
    if(NOT ENGINE_SHADERC_PATH OR NOT EXISTS ${ENGINE_SHADERC_PATH})
        message(FATAL_ERROR "ENGINE_SHADERC_PATH must point to a valid shaderc executable. Got: '${ENGINE_SHADERC_PATH}'")
    endif()
    
    if(NOT BGFX_SHADER_INCLUDE_DIR OR NOT EXISTS "${BGFX_SHADER_INCLUDE_DIR}/bgfx_shader.sh")
        message(FATAL_ERROR "BGFX_SHADER_INCLUDE_DIR must contain bgfx_shader.sh. Got: '${BGFX_SHADER_INCLUDE_DIR}'")
    endif()
    
    message(STATUS "Shader compiler: ${ENGINE_SHADERC_PATH}")
    message(STATUS "Shader includes: ${BGFX_SHADER_INCLUDE_DIR}")
    message(STATUS "Shader platform: ${SHADER_PLATFORM}")
    
    # Compile vertex shaders
    foreach(SHADER ${VERTEX_SHADERS})
        compile_shader(${SHADER} "vertex" ${SHADER_PROFILE_VS})
    endforeach()
    
    # Compile fragment shaders
    foreach(SHADER ${FRAGMENT_SHADERS})
        compile_shader(${SHADER} "fragment" ${SHADER_PROFILE_FS})
    endforeach()
    
    # Create a custom target for all shaders
    add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
endif()

# ============================================
# Main Executable
# ============================================

add_executable(${PROJECT_NAME}
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${STB_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    bgfx
    bx
)

# Make sure shaders are built before the executable
if(ENGINE_BUILD_SHADERS)
    add_dependencies(${PROJECT_NAME} shaders)
endif()

# ============================================
# Platform-specific settings
# ============================================

if(APPLE)
    # Link Metal and Cocoa frameworks
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework IOKit"
    )
    
    # Set RPATH for finding SDL2 at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "@executable_path"
        INSTALL_RPATH "@executable_path"
    )
endif()

# ============================================
# Debug settings (sanitizers)
# ============================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENGINE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address,undefined
        )
        message(STATUS "Sanitizers enabled (ASan + UBSan)")
    endif()
endif()

# ============================================
# Compiler warnings
# ============================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /wd4100  # unreferenced formal parameter
    )
endif()

# ============================================
# Copy shaders to build directory
# ============================================

# Create shaders/bin directory structure that matches what the code expects
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/bin"
    COMMENT "Copying compiled shaders to output directory"
)

# ============================================
# Summary
# ============================================

message(STATUS "")
message(STATUS "=== Pixel Sim Engine Configuration ===")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build shaders:     ${ENGINE_BUILD_SHADERS}")
message(STATUS "Sanitizers:        ${ENGINE_ENABLE_SANITIZERS}")
message(STATUS "Output directory:  ${CMAKE_BINARY_DIR}")
message(STATUS "")
