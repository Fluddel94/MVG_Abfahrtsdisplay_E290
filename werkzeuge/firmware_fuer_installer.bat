@echo off
rem Startet firmware_fuer_installer.ps1 (Doppelklick genuegt).
rem Umgeht die Ausfuehrungsrichtlinie nur fuer diesen einen Aufruf.
set FW_EXPORT_BAT=1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0firmware_fuer_installer.ps1"
echo.
pause
