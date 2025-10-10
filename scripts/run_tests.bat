@echo off
REM Run all tests

cd build
ctest --output-on-failure
pause
