@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -B build -S . -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON 2>&1 > build_log.txt
if errorlevel 1 (
    type build_log.txt
    exit /b 1
)
cmake --build build --config Release 2>&1 >> build_log.txt
if errorlevel 1 (
    type build_log.txt
    exit /b 1
)
type build_log.txt
echo Build completed successfully