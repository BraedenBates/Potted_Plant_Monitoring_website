@echo off
cd /d "%~dp0"
start "HTTP Server" cmd /k python -m http.server 8000 --bind 0.0.0.0
start "WebSocket Server" cmd /k python server.py
timeout /t 2 >nul
start http://[IP]:8000