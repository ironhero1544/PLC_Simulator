#!/bin/bash
# Format all C++ source files using clang-format

echo "Formatting all C++ files..."

# Format headers
find include/plc_emulator -name "*.h" -exec clang-format -i {} \;

# Format sources
find src -name "*.cpp" -o -name "*.cc" -exec clang-format -i {} \;

echo "✓ Formatting complete!"
