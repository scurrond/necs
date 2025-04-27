@echo off
echo === Building benchmarks ===

REM Go up one directory to include necs.hpp and model.hpp
set INCLUDES=-I..

REM Use C++20, enable warnings
set CXXFLAGS=-std=c++20 -O2 -Wall -Wextra

REM Source file
set SOURCE=main.cpp

REM Output
set OUT=-o benchmarks.exe

REM Compile
g++ %CXXFLAGS% %INCLUDES% %SOURCE% %OUT%

if %ERRORLEVEL% neq 0 (
    echo === Build failed ===
    pause
    exit /b %ERRORLEVEL%
)

echo === Build succeeded ===
.\benchmarks.exe
pause
