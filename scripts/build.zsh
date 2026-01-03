#!/usr/bin/env zsh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

# ----------------------------
# Config (override via env vars)
# ----------------------------
# CONFIG: Debug | Release | RelWithDebInfo | MinSizeRel
CONFIG="${CONFIG:-Debug}"

# Separate build dirs per config (avoids stale-cache pain)
CONFIG_LC="$(print -r -- "$CONFIG" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${BUILD_DIR:-build-${CONFIG_LC}}"
TOOLS_DIR="${TOOLS_DIR:-build_bgfx_tools}"

# Generator
GEN="${GEN:-Ninja}"

# Optional: set CLEAN=1 to wipe build dirs
CLEAN="${CLEAN:-0}"

# Optional: set SHADERS=0 to skip build-time shader compilation
SHADERS="${SHADERS:-1}"

# Optional: set SANITIZERS=0 to disable sanitizers in Debug (if your CMake option supports it)
SANITIZERS="${SANITIZERS:-1}"

# Deterministic include dir for bgfx shader includes (contains bgfx_shader.sh)
BGFX_INC="${BGFX_SHADER_INCLUDE_DIR:-$ROOT_DIR/third_party/bgfx.cmake/bgfx/src}"

# ----------------------------
# Validate CONFIG
# ----------------------------
case "$CONFIG" in
  Debug|Release|RelWithDebInfo|MinSizeRel) ;;
  *)
    echo "ERROR: CONFIG must be one of: Debug, Release, RelWithDebInfo, MinSizeRel" >&2
    exit 1
    ;;
esac

# ----------------------------
# vcpkg toolchain
# ----------------------------
TOOLCHAIN=""
if [[ -f "$ROOT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
  TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$ROOT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake"
elif [[ -n "${VCPKG_ROOT:-}" && -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
  TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
fi

# ----------------------------
# Clean (optional)
# ----------------------------
if [[ "$CLEAN" == "1" ]]; then
  echo "==> Cleaning build dirs..."
  rm -rf "$BUILD_DIR" "$TOOLS_DIR"
fi

# ----------------------------
# Build shaderc tool (incremental)
# ----------------------------
echo "==> Configure bgfx tools..."
cmake -S third_party/bgfx.cmake -B "$TOOLS_DIR" -G "$GEN" \
  -DBGFX_BUILD_TOOLS=ON \
  -DBGFX_BUILD_EXAMPLES=OFF \
  -DBGFX_BUILD_TESTS=OFF

echo "==> Build shaderc..."
cmake --build "$TOOLS_DIR" --target shaderc

# Find shaderc (your known mac layout first)
SHADERC=""
for p in \
  "$ROOT_DIR/$TOOLS_DIR/cmake/bgfx/shaderc" \
  "$ROOT_DIR/$TOOLS_DIR/bin/shaderc" \
  "$ROOT_DIR/$TOOLS_DIR/tools/shaderc/shaderc"
do
  if [[ -x "$p" ]]; then
    SHADERC="$p"
    break
  fi
done

if [[ -z "$SHADERC" ]]; then
  echo "ERROR: shaderc not found after build." >&2
  echo "Checked:" >&2
  echo "  $ROOT_DIR/$TOOLS_DIR/cmake/bgfx/shaderc" >&2
  echo "  $ROOT_DIR/$TOOLS_DIR/bin/shaderc" >&2
  echo "  $ROOT_DIR/$TOOLS_DIR/tools/shaderc/shaderc" >&2
  exit 1
fi

if [[ ! -f "$BGFX_INC/bgfx_shader.sh" ]]; then
  echo "ERROR: bgfx_shader.sh not found in: $BGFX_INC" >&2
  echo "Set BGFX_SHADER_INCLUDE_DIR to a dir containing bgfx_shader.sh" >&2
  exit 1
fi

echo "==> shaderc: $SHADERC"
echo "==> bgfx includes: $BGFX_INC"

# ----------------------------
# Configure + build engine
# ----------------------------
ENGINE_SHADERS_FLAG="ON"
if [[ "$SHADERS" == "0" ]]; then
  ENGINE_SHADERS_FLAG="OFF"
fi

ENGINE_SANITIZERS_FLAG="ON"
if [[ "$SANITIZERS" == "0" ]]; then
  ENGINE_SANITIZERS_FLAG="OFF"
fi

echo "==> Configure engine ($CONFIG)..."
cmake -S . -B "$BUILD_DIR" -G "$GEN" \
  $TOOLCHAIN \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DENGINE_BUILD_SHADERS="$ENGINE_SHADERS_FLAG" \
  -DENGINE_AUTO_BUILD_SHADERC=OFF \
  -DENGINE_SHADERC_PATH="$SHADERC" \
  -DBGFX_SHADER_INCLUDE_DIR="$BGFX_INC" \
  -DENGINE_ENABLE_SANITIZERS="$ENGINE_SANITIZERS_FLAG"

echo "==> Build engine ($CONFIG)..."
cmake --build "$BUILD_DIR"

# ----------------------------
# Copy assets to build dir (if assets folder exists)
# ----------------------------
if [[ -d "$ROOT_DIR/assets" ]]; then
  echo "==> Copying assets..."
  mkdir -p "$BUILD_DIR/assets"
  cp -r "$ROOT_DIR/assets/"* "$BUILD_DIR/assets/" 2>/dev/null || true
fi

echo "==> Done. Build dir: $BUILD_DIR"
echo ""
echo "To run:"
echo "  cd $BUILD_DIR && ./pixel_sim_engine"
