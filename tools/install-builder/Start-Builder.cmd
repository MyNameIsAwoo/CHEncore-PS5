@echo off
setlocal
cd /d "%~dp0"
py -3.12 -c "import sys,struct; sys.exit(not (sys.version_info[:2] == (3,12) and struct.calcsize('P') == 8))" >nul 2>nul
if errorlevel 1 (
  echo Source launchers require Python 3.12 x64.
  echo For Windows users, download the standalone CH Encore builder EXE instead.
  pause
  exit /b 1
)
if not exist .venv-py312\Scripts\python.exe py -3.12 -m venv .venv-py312
if not exist .venv-py312\Scripts\python.exe exit /b 1
.venv-py312\Scripts\python.exe -m pip install --disable-pip-version-check -r requirements.txt
if errorlevel 1 (
  pause
  exit /b 1
)
.venv-py312\Scripts\python.exe builder.py
if errorlevel 1 pause
