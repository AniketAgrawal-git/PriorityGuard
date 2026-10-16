@echo off
setlocal enabledelayedexpansion
title PriorityGuard Launcher

echo =====================================================================
echo           PriorityGuard: Priority Inversion Analysis Framework
echo =====================================================================
echo.

REM 1. Check Python
where python >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python was not found in your PATH.
    echo Please install Python 3.9+ and add it to PATH.
    pause
    exit /b 1
)
echo [OK] Python detected.

REM 2. Check C++ Compiler
where g++ >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] g++ compiler not found in PATH.
    echo If the engine binary is already built, execution will continue.
) else (
    echo [OK] g++ compiler detected.
)

REM 3. Check / Compile C++ Engine
set ENGINE_EXE=%~dp0engine\priorityguard_engine.exe
if not exist "%ENGINE_EXE%" (
    echo [BUILD] Engine binary not found. Compiling C++ engine...
    g++ -std=c++17 -O2 -o "%ENGINE_EXE%" "%~dp0engine\src\main.cpp" -I"%~dp0engine\include" -I"%~dp0engine\third_party"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Failed to compile C++ engine.
        pause
        exit /b 1
    )
    echo [OK] C++ engine built successfully.
) else (
    echo [OK] C++ engine binary verified: %ENGINE_EXE%
)

REM 4. Check & Install Python Dependencies
echo [CHECK] Verifying Python dependencies (streamlit, plotly, pandas)...
python -c "import streamlit, plotly, pandas" >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [INSTALL] Installing missing Python packages...
    python -m pip install -r "%~dp0requirements.txt"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Failed to install Python dependencies.
        pause
        exit /b 1
    )
)
echo [OK] Python dependencies verified.

REM 5. Launch Streamlit Application
echo.
echo =====================================================================
echo Starting PriorityGuard Dashboard...
echo Local URL: http://localhost:8501
echo Press Ctrl+C in this console to terminate the server.
echo =====================================================================
echo.

cd /d "%~dp0frontend"
streamlit run app.py

pause

