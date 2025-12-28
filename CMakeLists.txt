
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
