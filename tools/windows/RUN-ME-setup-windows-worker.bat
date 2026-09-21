@echo off
rem Double-click this. It runs setup-windows-worker.ps1 from the same folder, bypassing
rem PowerShell's script execution policy for that one run. The script asks for admin
rem rights itself (UAC prompt). Arguments pass through, e.g.:  RUN-ME-setup-windows-worker.bat -Build
rem See the .ps1 for what it does. Safe to run again.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-windows-worker.ps1" %*
