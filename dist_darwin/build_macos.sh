#!/bin/bash
forgevm build
clang++ -std=c++17 -O2 -arch arm64 app_main.cpp -L. -o test
mkdir -p test.app/Contents/MacOS
cp test test.app/Contents/MacOS/
