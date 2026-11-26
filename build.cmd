@echo off
chcp 65001>nul
rem Сборка библиотеки Elements и примеров (Release, статический CRT).
rem Запуск из PowerShell:  & ".\build.cmd"
setlocal enableextensions
pushd "%~dp0"

set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -find VC\Auxiliary\Build\vcvars64.bat`) do set "VCVARS=%%i"
)
if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo [!] vcvars64.bat not found. Install Visual Studio with the C++ workload. & goto :fail )
call "%VCVARS%" >nul

if not exist build mkdir build
if not exist lib mkdir lib
if not exist bin mkdir bin
del /q build\*.obj 2>nul

set "CFLAGS=/nologo /c /std:c17 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /W3 /I include"

echo === Building the Elements library ===
cl %CFLAGS% /Fo:build\ src\*.c
if errorlevel 1 ( echo [!] Library compile error & goto :fail )
lib /nologo /out:lib\Elements.lib build\el_*.obj
if errorlevel 1 ( echo [!] lib error & goto :fail )

set "LIBS=lib\Elements.lib gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib"
echo === Building the examples ===
for %%f in (examples\*.c) do (
  echo   %%~nf
  cl /nologo /std:c17 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /W3 /I include /Fo:build\ /Fe:bin\%%~nf.exe %%f %LIBS% /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
  if errorlevel 1 ( echo [!] Example build error: %%~nf & goto :fail )
)

echo.
echo Done. Library: lib\Elements.lib  (include include\elements.h)
echo Examples are in bin\
popd
endlocal
exit /b 0

:fail
popd
endlocal
exit /b 1
