@echo off
rem Double-click to build (if needed) and start a skirmish against the AI.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\play_vs_ai.ps1" %*
if errorlevel 1 pause
