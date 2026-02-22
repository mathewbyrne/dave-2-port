@echo off
setlocal

set TARGET=%1
if "%TARGET%"=="" set TARGET=game

if not exist build mkdir build
pushd build

if "%TARGET%"=="unpack" (
    cl -nologo -FC -Zi -W4 -std:c17 -TP ..\src\unpack.cpp -Fe:unpack.exe
) else if "%TARGET%"=="game" (
    cl -nologo -FC -Zi -W4 -std:c17 -TP ..\src\platform_win32.cpp -Fe:dave2.exe /link user32.lib gdi32.lib
) else (
    echo Unknown target "%TARGET%".
    echo Usage: build.bat [game^|unpack]
    popd
    exit /b 1
)

popd
endlocal
