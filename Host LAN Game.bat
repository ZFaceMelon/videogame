@echo off
cd /d "%~dp0"
set VIDEOGAME_NET_MODE=host
echo.
echo === HOSTING THE PHARAOH FIGHT ===
echo Give your brother the IPv4 Address for your Wi-Fi adapter:
ipconfig | findstr /C:"IPv4 Address"
echo.
echo If Windows asks, allow the game on Private networks.
echo Leave this window open while you play.
echo.
"build\Release\videogame_cxx.exe"
pause
