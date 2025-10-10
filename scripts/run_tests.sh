#!/bin/bash
# Run all tests

cd build || exit 1
ctest --output-on-failure
