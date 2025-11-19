#!/bin/bash
# TrinityCore PlayerBot - Dependency Installation Script
# Runs automatically on SessionStart in Claude Code Web

set -e

echo "=== TrinityCore PlayerBot Dependency Setup ==="
echo ""

# Only run in remote (web) environments
if [ "$CLAUDE_CODE_REMOTE" != "true" ]; then
  echo "Skipping dependency installation (local environment)"
  exit 0
fi

echo "Running in Claude Code Web environment - checking dependencies..."

# Navigate to TrinityCore directory
TRINITY_DIR="${CLAUDE_PROJECT_DIR:-/home/user/TrinityCore}"
cd "$TRINITY_DIR"

# ============================================================================
# STEP 1: Initialize TBB and phmap (PlayerBot enterprise dependencies)
# ============================================================================

echo ""
echo "Step 1: Checking PlayerBot enterprise dependencies (TBB, phmap)..."

TBB_DIR="${TRINITY_DIR}/src/modules/Playerbot/deps/tbb"
PHMAP_DIR="${TRINITY_DIR}/src/modules/Playerbot/deps/phmap"

TBB_OK=false
PHMAP_OK=false

if [ -f "${TBB_DIR}/include/tbb/version.h" ]; then
  echo "✅ TBB already initialized"
  TBB_OK=true
fi

if [ -f "${PHMAP_DIR}/parallel_hashmap/phmap.h" ]; then
  echo "✅ phmap already initialized"
  PHMAP_OK=true
fi

if [ "$TBB_OK" = false ] || [ "$PHMAP_OK" = false ]; then
  echo "📦 Initializing PlayerBot vendored dependencies (TBB, phmap)..."
  git submodule update --init --recursive src/modules/Playerbot/deps/
  echo "✅ TBB and phmap initialized successfully!"
fi

# ============================================================================
# STEP 2: Set up Boost 1.83.0
# ============================================================================

echo ""
echo "Step 2: Checking Boost 1.83.0..."

# Check if Boost is already set up
if [ -d "/home/user/boost_1_83_0" ] && [ -f "/home/user/boost_1_83_0/stage/lib/libboost_filesystem.a" ]; then
  echo "✅ Boost 1.83.0 already installed"
  export BOOST_ROOT=/home/user/boost_1_83_0
  echo "export BOOST_ROOT=/home/user/boost_1_83_0" >> "${CLAUDE_ENV_FILE}"

  echo ""
  echo "✅ All dependencies ready!"
  echo "   - TBB: Vendored (git submodule)"
  echo "   - phmap: Vendored (git submodule)"
  echo "   - Boost: /home/user/boost_1_83_0"
  echo ""
  echo "Ready to build TrinityCore PlayerBot module!"
  exit 0
fi

echo "📦 Setting up Boost 1.83.0 from GitHub..."

cd /home/user

# Clone Boost if not already present
if [ ! -d "boost_1_83_0" ]; then
  echo "Cloning Boost 1.83.0..."
  git clone --depth 1 --branch boost-1.83.0 https://github.com/boostorg/boost.git boost_1_83_0
fi

cd boost_1_83_0

# Initialize required submodules
echo "Initializing Boost submodules..."
git submodule update --init --depth 1 \
  libs/filesystem \
  libs/program_options \
  libs/regex \
  libs/locale \
  libs/thread \
  libs/system \
  libs/config \
  libs/core \
  libs/detail \
  libs/type_traits \
  libs/static_assert \
  libs/preprocessor \
  libs/mpl \
  libs/assert \
  libs/throw_exception \
  tools/build \
  tools/boost_install

# Bootstrap if needed
if [ ! -f "b2" ]; then
  echo "Bootstrapping Boost build system..."
  ./bootstrap.sh --with-libraries=filesystem,program_options,regex,locale,thread
fi

# Generate headers
echo "Generating Boost headers..."
./b2 headers

# Build required libraries
echo "Building Boost libraries (this may take 5-10 minutes)..."
./b2 --with-filesystem --with-program_options --with-regex --with-locale --with-thread \
  -j4 variant=release link=static runtime-link=shared

echo ""
echo "✅ All dependencies setup complete!"
echo ""

# Export BOOST_ROOT for the session
export BOOST_ROOT=/home/user/boost_1_83_0
echo "export BOOST_ROOT=/home/user/boost_1_83_0" >> "${CLAUDE_ENV_FILE}"

echo "Dependency Summary:"
echo "   - TBB: ${TBB_DIR} (vendored)"
echo "   - phmap: ${PHMAP_DIR} (vendored)"
echo "   - Boost: /home/user/boost_1_83_0 (built from source)"
echo "   - BOOST_ROOT=/home/user/boost_1_83_0"
echo ""
echo "🚀 Ready to build TrinityCore PlayerBot module!"

exit 0
