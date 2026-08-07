@echo off
cd /d "%~dp0"
echo.
echo === JOIN THE PHARAOH FIGHT ===
set /p VIDEOGAME_HOST_IP=Enter the host computer's IPv4 Address: 
set VIDEOGAME_NET_MODE=client
echo Connecting to %VIDEOGAME_HOST_IP%...
"build\Release\videogame_cxx.exe"
pause
