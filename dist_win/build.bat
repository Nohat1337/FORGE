@echo off
forgevm build
cl /O2 /EHsc app_main.cpp forge_app.lib /Fe:test.exe
