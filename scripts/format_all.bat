@echo off
REM Format all C++ source files using clang-format

echo Formatting all C++ files...

REM Format headers
for /r include\plc_emulator %%f in (*.h) do (
    clang-format -i "%%f"
)

REM Format sources
for /r src %%f in (*.cpp *.cc) do (
    clang-format -i "%%f"
)

echo Done! Formatting complete.
pause
