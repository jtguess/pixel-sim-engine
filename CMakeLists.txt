
cmake_minimum_required(VERSION 3.24)
project(pixel_sim_engine LANGUAGES CXX)

# ---- C++ / tooling ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----
option(ENGINE_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug" ON)

# ---- Dependencies ----
# vcpkg toolchain (you can also pass -DCMAKE_TOOLCHAIN_FILE=... on the command line)
if (DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(bgfx CONFIG REQUIRED)

# ---- Target ----
add_executable(engine
  src/main.cpp
)


# ---- bgfx shader compilation (Metal for macOS) ----
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_BIN_DIR "${CMAKE_SOURCE_DIR}/shaders/bin")
file(MAKE_DIRECTORY "${SHADER_BIN_DIR}")

# Try to locate shaderc from vcpkg.
# vcpkg toolchain often sets VCPKG_INSTALLED_DIR; fallback to repo-local vcpkg installed.
if (DEFINED VCPKG_INSTALLED_DIR)
  set(_vcpkg_installed "${VCPKG_INSTALLED_DIR}")
else()
  set(_vcpkg_installed "${CMAKE_SOURCE_DIR}/vcpkg/installed")
endif()

# Triplet folder name differs (arm64-osx / x64-osx). Search it.
find_program(SHADERC_EXECUTABLE shaderc
  PATHS
    "${_vcpkg_installed}/arm64-osx/tools/bgfx"
    "${_vcpkg_installed}/x64-osx/tools/bgfx"
    "${_vcpkg_installed}/arm64-osx/tools/bgfx/shaderc"
    "${_vcpkg_installed}/x64-osx/tools/bgfx/shaderc"
  NO_DEFAULT_PATH
)

# If not found in the above, fall back to PATH search (some installs put it on PATH)
if (NOT SHADERC_EXECUTABLE)
  find_program(SHADERC_EXECUTABLE shaderc)
endif()

if (NOT SHADERC_EXECUTABLE)
  message(FATAL_ERROR "bgfx shaderc not found. Check vcpkg bgfx tools install.")
endif()

set(BLIT_VS "${SHADER_SRC_DIR}/blit.vs.sc")
set(BLIT_FS "${SHADER_SRC_DIR}/blit.fs.sc")

set(BLIT_VS_BIN "${SHADER_BIN_DIR}/vs_blit.bin")
set(BLIT_FS_BIN "${SHADER_BIN_DIR}/fs_blit.bin")

add_custom_command(
  OUTPUT "${BLIT_VS_BIN}"
  COMMAND "${SHADERC_EXECUTABLE}"
          -f "${BLIT_VS}"
          -o "${BLIT_VS_BIN}"
          --platform osx
          --type vertex
          --profile metal
          -i "${_vcpkg_installed}/arm64-osx/include/bgfx" # ok if missing; shaderc has its own includes too
          -i "${_vcpkg_installed}/x64-osx/include/bgfx"
  DEPENDS "${BLIT_VS}"
  VERBATIM
)

add_custom_command(
  OUTPUT "${BLIT_FS_BIN}"
  COMMAND "${SHADERC_EXECUTABLE}"
          -f "${BLIT_FS}"
          -o "${BLIT_FS_BIN}"
          --platform osx
          --type fragment
          --profile metal
          -i "${_vcpkg_installed}/arm64-osx/include/bgfx"
          -i "${_vcpkg_installed}/x64-osx/include/bgfx"
  DEPENDS "${BLIT_FS}"
  VERBATIM
)

add_custom_target(shaders ALL
  DEPENDS "${BLIT_VS_BIN}" "${BLIT_FS_BIN}"
)

add_dependencies(engine shaders)

# SDL: vcpkg typically provides SDL2::SDL2 target
target_link_libraries(engine PRIVATE
  SDL2::SDL2
  bgfx::bgfx
)

# On macOS, avoid SDL redefining main() behind your back
if (APPLE)
  target_compile_definitions(engine PRIVATE SDL_MAIN_HANDLED)
endif()

# ---- Sanitizers (no-mystery, target-local) ----
if (ENGINE_ENABLE_SANITIZERS)
  target_compile_options(engine PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>>:
      -fno-omit-frame-pointer
      -fsanitize=address,undefined
    >
  )
  target_link_options(engine PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>>:
      -fsanitize=address,undefined
    >
  )
endif()
