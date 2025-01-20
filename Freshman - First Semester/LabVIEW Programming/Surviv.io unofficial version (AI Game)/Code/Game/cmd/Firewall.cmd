@echo off

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo 管理員權限不足，正在嘗試提升權限...
    powershell -Command "Start-Process cmd -ArgumentList '/c %~f0' -Verb runAs"
    exit /b
)

netsh advfirewall firewall add rule name="Open UDP Port 52023" dir=in action=allow protocol=UDP localport=52023
netsh advfirewall firewall add rule name="Open UDP Port 52023" dir=out action=allow protocol=UDP localport=52023