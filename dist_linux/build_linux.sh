#!/bin/bash
forgevm build
 g++ -std=c++17 -O2 -static app_main.cpp forge_core.a -o test 2>/dev/null || g++ -std=c++17 -O2 app_main.cpp -L. -lforge_app -o test
strip test
