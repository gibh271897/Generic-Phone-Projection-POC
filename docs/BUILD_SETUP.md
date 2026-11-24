# Build Environment Setup Required

## Missing Components
- **C++ Compiler (cl.exe / clang-cl.exe)**: Not installed in current BuildTools
- **Windows SDK**: May be missing
- **vswhere.exe**: Not available (needed for VS detection)

## Required Installation

### Option 1: Install Visual Studio Community (Recommended)
```powershell
# Via winget
winget install Microsoft.VisualStudio.2022.Community

# Or download from https://visualstudio.microsoft.com/downloads/
# Select "Desktop development with C++" workload
```

### Option 2: Install BuildTools with C++ Workload
```powershell
# Download vs_buildtools.exe from https://visualstudio.microsoft.com/downloads/
# Run with:
vs_buildtools.exe --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Workload.MSBuildTools --quiet
```

### Option 3: Use clang-cl from LLVM (if available)
```powershell
# Install LLVM
winget install LLVM.LLVM

# Use with CMake:
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang-cl -G "Ninja"
```

## Verification Commands (After Installation)
```powershell
# Verify C++ compiler
cl /?

# Verify CMake finds VS
cmake -B build -S . -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON

# Build
cmake --build build --config Release

# Test
ctest --test-dir build --output-on-failure

# Run demo
./build/bin/examples/demo_app
```



## Next Session: Build & Verify
1. Install Visual Studio 2022 with C++ workload
2. Run `cmake -B build -S . -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON`
3. Run `cmake --build build --config Release`
4. Run ` ctest --test-dir build -C Release --output-on-failure`
5. Run `build\bin\Release\demo_app.exe`
6. Verify all 36 tests pass
7. Initialize git repository and commit