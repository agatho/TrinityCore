# Build Environment Setup Instructions

**Date:** 2025-09-03  
**Status:** Requires Admin Access  
**Current User:** claude (uid=1000, no sudo access)

## Required Build Tools

### Essential Packages Needed
```bash
# Install build essentials (requires sudo/root access)
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libmysqlclient-dev \
    libssl-dev \
    libreadline-dev \
    zlib1g-dev \
    libbz2-dev \
    libboost-all-dev \
    git
```

### TrinityCore Specific Requirements
Based on CLAUDE.md documentation, TrinityCore requires:
- **CMake 3.24+** (for build system)
- **GCC 11+** (C++20 support)
- **MySQL client libraries** (for database connectivity)
- **Boost libraries** (for advanced C++ features)
- **OpenSSL** (for cryptographic functions)

## Current Environment Status

### ✅ Available
- Debian 12 (bookworm) package management (apt/dpkg)
- Basic gcc-12-base libraries installed
- Git repository access working
- File system access for development

### ❌ Missing (Critical)
- **gcc/g++** compilers
- **cmake** build system  
- **make** build tool
- **libmysqlclient-dev** MySQL development libraries
- **libboost-all-dev** Boost C++ libraries

## Alternative Setup Options

### Option 1: Request Admin Access
Ask system administrator to install required packages:
```bash
sudo apt-get install -y build-essential cmake libmysqlclient-dev libssl-dev libboost-all-dev
```

### Option 2: User-space Installation (Complex)
- Use conda/miniconda for user-space package management
- Install build tools in user directory
- More complex but doesn't require sudo

### Option 3: Container/Docker Approach  
- Use TrinityCore's Docker configuration in `contrib/Docker/`
- Requires Docker access with sufficient privileges

## Build Validation Plan

Once build environment is available, execute these tests in order:

### 1. Baseline Compilation Test
```bash
cd build
cmake .. -DWITH_PLAYERBOTS=OFF -DTOOLS=1 -DSERVERS=1 -DBUILD_TESTING=1
make -j$(nproc)
```
**Expected:** Clean compilation with no errors (validates no regression)

### 2. Playerbot Integration Test
```bash
# Clear cmake cache
rm -rf *
cmake .. -DWITH_PLAYERBOTS=ON -DTOOLS=1 -DSERVERS=1 -DBUILD_TESTING=1
make -j$(nproc)
```
**Expected:** Clean compilation with playerbot database integration

### 3. Feature Toggle Test
```bash
# Test toggle functionality
cmake .. -DWITH_PLAYERBOTS=OFF
make clean && make -j$(nproc)
# Should compile without playerbot references

cmake .. -DWITH_PLAYERBOTS=ON  
make clean && make -j$(nproc)
# Should compile with playerbot integration
```

### 4. Runtime Database Test
```bash
# Test database connection (requires MySQL running)
./bin/worldserver --dry-run  # or equivalent
```
**Expected:** Database connections establish without errors

## Build Environment Commands Summary

**For next session with admin access:**
```bash
# 1. Install build tools
sudo apt-get update && sudo apt-get install -y build-essential cmake libmysqlclient-dev libssl-dev libboost-all-dev

# 2. Test baseline compilation
cd /workspace/TrinityCore/build
cmake .. -DWITH_PLAYERBOTS=OFF -DTOOLS=1 -DSERVERS=1
make -j$(nproc)

# 3. Test playerbot integration  
rm -rf *
cmake .. -DWITH_PLAYERBOTS=ON -DTOOLS=1 -DSERVERS=1
make -j$(nproc)

# 4. Verify success
echo "Build validation complete - Phase 1.1 verified"
```

## Integration Validation Checklist

### Pre-Build Code Review ✅
- [x] All files follow TrinityCore patterns
- [x] Conditional compilation properly implemented  
- [x] Template instantiations correct
- [x] Database loader integration complete
- [x] SQL schemas properly formatted

### Build Testing (Pending Environment)
- [ ] Baseline compilation succeeds
- [ ] Playerbot compilation succeeds  
- [ ] Feature toggle works correctly
- [ ] No compilation warnings/errors
- [ ] Link phase completes successfully

### Runtime Testing (Future)
- [ ] Database connections establish
- [ ] Prepared statements initialize
- [ ] Configuration loading works
- [ ] No memory leaks detected
- [ ] Performance impact measured

**Status:** Code integration appears complete and correct, pending build environment for validation.