# TrinityCore PlayerBot - Build System Guide

## Claude Code Web - Iterative Compilation Environment

This guide explains how to use the build system for iterative development and compilation of the PlayerBot module in Claude Code Web.

---

## 🎯 Quick Start

### 1. Quick Syntax Check (Fastest - 10-30 seconds)

Check recently modified files:
```bash
./check-playerbot-syntax.sh recent
```

Check all PlayerBot files:
```bash
./check-playerbot-syntax.sh all
```

Check only git-staged files:
```bash
./check-playerbot-syntax.sh staged
```

Check specific file:
```bash
./check-playerbot-syntax.sh src/modules/Playerbot/Social/UnifiedLootManager.cpp
```

### 2. Full Build (Slower - 5-15 minutes)

First time setup and build:
```bash
./build-playerbot.sh
```

### 3. Error Analysis & Fixes

After a failed build, analyze errors:
```bash
./fix-playerbot-errors.sh
```

---

## 📋 Available Scripts

### `build-playerbot.sh` - Full Compilation

**Purpose**: Complete CMake configuration and compilation of PlayerBot module

**What it does**:
1. ✅ Checks all build dependencies (cmake, g++, libraries)
2. ✅ Configures CMake with PlayerBot enabled
3. ✅ Builds the game library (includes PlayerBot)
4. ✅ Generates detailed build logs
5. ✅ Reports compilation statistics

**Output**:
- `build-playerbot/` - CMake build directory
- `build-playerbot/cmake-config.log` - CMake configuration output
- `build-playerbot/build.log` - Full compilation log
- `build-playerbot/compile_commands.json` - Compilation database for IDE integration

**Typical execution time**: 5-15 minutes (first run), 1-5 minutes (incremental)

**Usage**:
```bash
./build-playerbot.sh
```

---

### `check-playerbot-syntax.sh` - Fast Syntax Validation

**Purpose**: Quick syntax-only checks without full compilation

**What it does**:
1. ✅ Runs g++ syntax-only compilation (`-fsyntax-only`)
2. ✅ Checks include paths and header availability
3. ✅ Detects syntax errors, type errors, missing includes
4. ❌ Does NOT detect linker errors or missing implementations

**Modes**:
- `recent` - Check files modified in last 24 hours (default)
- `all` - Check all .cpp files in PlayerBot module
- `staged` - Check only git-staged files
- `<file>` - Check specific file

**Typical execution time**: 10-30 seconds

**Usage**:
```bash
# Check recent changes (fast iteration)
./check-playerbot-syntax.sh recent

# Check everything before committing
./check-playerbot-syntax.sh all

# Check only staged changes
./check-playerbot-syntax.sh staged

# Check specific file
./check-playerbot-syntax.sh src/modules/Playerbot/Dungeon/DungeonBehavior.cpp
```

**Use cases**:
- ✅ Rapid iteration during development
- ✅ Pre-commit validation
- ✅ Checking if includes are correct
- ✅ Verifying syntax before full build

---

### `fix-playerbot-errors.sh` - Error Analysis & Suggestions

**Purpose**: Analyze compilation errors and suggest fixes

**What it does**:
1. ✅ Categorizes errors by type (includes, undeclared, members, types, etc.)
2. ✅ Counts errors per category
3. ✅ Shows most problematic files
4. ✅ Suggests specific fixes for common errors
5. ✅ Provides file paths for editing

**Error categories detected**:
- 🔴 **Missing includes**: Suggests which headers to add
- 🔴 **Undeclared identifiers**: Suggests includes or forward declarations
- 🔴 **Missing members**: Indicates wrong class/method name
- 🔴 **Type errors**: Missing type definitions
- 🟡 **Ambiguous references**: Suggests namespace qualification
- 🔴 **Undefined references**: Linker errors (missing implementations)

**Typical execution time**: < 5 seconds

**Usage**:
```bash
# Run after a failed build
./build-playerbot.sh
./fix-playerbot-errors.sh
```

**Example output**:
```
Error Summary:
  Undefined references: 3
  Missing members: 5
  Undeclared identifiers: 12
  Missing includes: 8
  Type errors: 2
  Ambiguous references: 1

════════════════════════════════════════
MISSING INCLUDE FILES (8 errors)
════════════════════════════════════════

  Missing: GroupCoordinator.h
  Add: #include "../Advanced/GroupCoordinator.h"

  Missing: Random.h
  Add: #include "Random.h"
```

---

## 🔄 Iterative Development Workflow

### Recommended workflow for fixing compilation errors:

```bash
# 1. Make code changes in Claude Code

# 2. Quick syntax check (10-30 seconds)
./check-playerbot-syntax.sh recent

# 3. If syntax is OK, do full build (5-15 minutes)
./build-playerbot.sh

# 4. If build fails, analyze errors
./fix-playerbot-errors.sh

# 5. Fix errors based on suggestions

# 6. Repeat from step 2 until successful
```

### Fast iteration cycle (for rapid development):

```bash
# Edit files...

# Quick check
./check-playerbot-syntax.sh recent

# Fix errors

# Quick check again
./check-playerbot-syntax.sh recent

# Once syntax is clean, do full build
./build-playerbot.sh
```

---

## 📦 Dependencies

### Required packages:

**Build Tools** (already installed in Claude Code Web):
- ✅ `cmake` (3.28.3+)
- ✅ `g++` (13.3.0+) - C++20 support
- ✅ `make`
- ✅ `ninja` (optional, faster builds)

**System Libraries** (already installed in Claude Code Web):
- ✅ `libboost-dev` (1.83.0)
- ✅ `libssl-dev` (OpenSSL 3.0)
- ✅ `libreadline-dev`
- ✅ `libmysqlclient-dev` (if available)

**PlayerBot Enterprise Dependencies** (automatically initialized):
- 🔄 **Intel TBB** (Threading Building Blocks) - Vendored via git submodule
- 🔄 **phmap** (Parallel Hashmap) - Vendored via git submodule

> **Note**: TBB and phmap are **CRITICAL** dependencies. The build script will automatically initialize them from git submodules if not found (zero installation required).

### Automatic Dependency Initialization:

On first run, the build script will:
1. Check for TBB and phmap in vendored locations
2. If not found, automatically run `git submodule update --init --recursive`
3. Download and initialize both dependencies (~50-100MB)
4. Proceed with build (zero manual installation required)

**First-time run example**:
```bash
./build-playerbot.sh

# Output:
# [INFO] Checking PlayerBot enterprise dependencies (TBB, phmap)...
# [WARNING] ⚠️  TBB not found - will initialize git submodules
# [WARNING] ⚠️  phmap not found - will initialize git submodules
# [INFO] Initializing PlayerBot vendored dependencies (TBB, phmap)...
# [SUCCESS] ✅ Git submodules initialized successfully!
# [SUCCESS] ✅ TBB and phmap are now available (zero system installation required)
```

### Check dependencies:

```bash
./build-playerbot.sh
# Will check all dependencies and initialize git submodules if needed
# Fails fast with clear error messages if critical dependencies are missing
```

---

## 🐛 Common Errors & Fixes

### 1. Missing Include Errors

**Error**:
```
error: 'GetBotAI' was not declared in this scope
```

**Fix**:
```cpp
#include "Core/PlayerBotHelpers.h"
```

---

### 2. Missing GroupCoordinator

**Error**:
```
error: 'GroupCoordinator' has not been declared
```

**Fix**:
```cpp
#include "../Advanced/GroupCoordinator.h"
```

Or add namespace:
```cpp
Advanced::GroupCoordinator* coord = ...
```

---

### 3. Missing TacticalCoordinator

**Error**:
```
error: 'TacticalCoordinator' was not declared
```

**Fix**:
```cpp
#include "../Advanced/TacticalCoordinator.h"
```

---

### 4. urand not found

**Error**:
```
error: 'urand' was not declared in this scope
```

**Fix**:
```cpp
#include "Random.h"
```

---

### 5. ObjectAccessor errors

**Error**:
```
error: 'ObjectAccessor' has not been declared
```

**Fix**:
```cpp
#include "ObjectAccessor.h"
```

---

### 6. Undefined reference (linker error)

**Error**:
```
undefined reference to `Playerbot::UnifiedLootManager::HandleMasterLoot(...)`
```

**Cause**: Method declared in .h but not implemented in .cpp

**Fix**: Implement the method in the .cpp file

---

### 7. Ambiguous reference

**Error**:
```
error: reference to 'LootRoll' is ambiguous
```

**Fix**: Use fully qualified name:
```cpp
Playerbot::LootRoll roll;
```

---

## 📊 Build Output Explanation

### Successful Build:

```
[INFO] ============================================
[INFO] Step 1: Checking build dependencies...
[INFO] ============================================
[SUCCESS] All dependencies found!
[INFO] CMake version: cmake version 3.28.3

[INFO] ============================================
[INFO] Step 2: Configuring CMake...
[INFO] ============================================
[INFO] Running CMake configuration...
[SUCCESS] CMake configuration complete!

[INFO] ============================================
[INFO] Step 4: Starting incremental compilation...
[INFO] ============================================
[INFO] Using 4 cores for parallel build
[INFO] Building game library with PlayerBot module...
[SUCCESS] Build completed successfully!

[INFO] ============================================
[INFO] Step 5: Analyzing build results...
[INFO] ============================================
[INFO] Compilation statistics:
[INFO]   - Errors: 0
[INFO]   - Warnings: 23
[SUCCESS] ============================================
[SUCCESS] BUILD SUCCESSFUL!
[SUCCESS] ============================================
```

### Failed Build:

```
[ERROR] ============================================
[ERROR] BUILD FAILED - Error Summary:
[ERROR] ============================================

[INFO] First 20 compilation errors:
error: 'GetBotAI' was not declared in this scope
error: 'GroupCoordinator' has not been declared
error: 'urand' was not declared in this scope
...

[INFO] Full error log saved to: /home/user/TrinityCore/build-playerbot/build.log
[INFO] To view full errors: cat /home/user/TrinityCore/build-playerbot/build.log | grep -A 3 'error:'
```

---

## 🎓 Advanced Usage

### View compile commands (for IDE integration):

```bash
cat build-playerbot/compile_commands.json | jq '.[0]'
```

### Rebuild from scratch:

```bash
rm -rf build-playerbot/
./build-playerbot.sh
```

### Check specific error in detail:

```bash
cat build-playerbot/build.log | grep -A 10 "UnifiedLootManager.cpp"
```

### Count errors per file:

```bash
grep "error:" build-playerbot/build.log | \
    grep -oP '/home/user/TrinityCore/[^:]+' | \
    sort | uniq -c | sort -rn
```

---

## 💡 Tips for Claude Code Web

### 1. Use syntax checks during development

Don't wait for full builds - use `check-playerbot-syntax.sh` frequently:

```bash
# After editing a file
./check-playerbot-syntax.sh src/modules/Playerbot/Social/UnifiedLootManager.cpp
```

### 2. Fix errors in batches

Use `fix-playerbot-errors.sh` to identify all instances of the same error type and fix them together.

### 3. Test incremental changes

Build after each logical change rather than making many changes at once:

```bash
# Make one change
# Quick syntax check
./check-playerbot-syntax.sh recent
# Full build if syntax OK
./build-playerbot.sh
```

### 4. Monitor warnings

Even if build succeeds, review warnings:

```bash
cat build-playerbot/build.log | grep "warning:" | head -20
```

### 5. Use grep for specific errors

```bash
# Find all "undefined reference" errors
grep "undefined reference" build-playerbot/build.log

# Find errors in specific file
grep "UnifiedLootManager" build-playerbot/build.log | grep "error:"
```

---

## 📝 Build System Internals

### CMake Configuration

Location: `build-playerbot/`

Key CMake variables:
- `CMAKE_BUILD_TYPE=Debug` - Debug symbols enabled
- `CMAKE_CXX_STANDARD=20` - C++20 standard
- `BUILD_PLAYERBOT=1` - PlayerBot module enabled
- `TOOLS=0` - Skip tool compilation
- `SCRIPTS=1` - Include script modules
- `WITH_WARNINGS=1` - Enable all warnings

### Compilation Database

Generated at: `build-playerbot/compile_commands.json`

Used by:
- IDEs (VSCode, CLion)
- Linters (clang-tidy)
- Static analyzers

### Parallel Builds

Default: Uses all CPU cores (`nproc`)

Override:
```bash
CORES=2 ./build-playerbot.sh
```

---

## 🔧 Troubleshooting

### Script won't run

```bash
chmod +x build-playerbot.sh check-playerbot-syntax.sh fix-playerbot-errors.sh
```

### CMake cache issues

```bash
rm -rf build-playerbot/CMakeCache.txt
./build-playerbot.sh
```

### Missing dependencies

```bash
# Check what's missing
./build-playerbot.sh

# Install (if needed, may require sudo)
sudo apt-get install cmake g++ make libboost-dev libssl-dev libreadline-dev
```

### Build hangs

- Kill process: `Ctrl+C`
- Check system resources
- Reduce parallel jobs: Edit `CORES` in script

---

## 📖 Related Documentation

- `PHASE7_GROUP_OPERATIONS_COMPLETE.md` - Phase 7 implementation details
- `GROUP_OPERATIONS_REVISED.md` - Coordinator architecture
- `LEGACY_CALL_MIGRATION_COMPLETE.md` - Migration documentation

---

*Last Updated: 2025-01-19*
*For use with Claude Code Web iterative development*
