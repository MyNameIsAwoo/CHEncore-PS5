@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  echo Install Python 3.10 or newer first, then run this again.
  pause
  exit /b 1
)
if not exist .venv\Scripts\python.exe py -m venv .venv
if not exist .venv\Scripts\python.exe exit /b 1
.venv\Scripts\python.exe -m pip install --disable-pip-version-check -r requirements.txt
if errorlevel 1 (
  pause
  exit /b 1
)
.venv\Scripts\python.exe create_asset_pack.py
pause
