@echo off
chcp 65001>nul
rem Сборка библиотеки Elements и примеров (Release, статический CRT).
rem Запуск из PowerShell:  & ".\build.cmd"
setlocal enableextensions
pushd "%~dp0"

set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -find VC\Auxiliary\Build\vcvars64.bat`) do set "VCVARS=%%i"
)
if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo [!] Не найден vcvars64.bat. Укажите путь к Visual Studio. & goto :fail )
call "%VCVARS%" >nul

if not exist build mkdir build
if not exist lib mkdir lib
if not exist bin mkdir bin

set "CFLAGS=/nologo /c /EHsc /std:c++17 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /W3 /I include"

echo === Сборка библиотеки Elements ===
cl %CFLAGS% /Fo:build\ src\*.cpp
if errorlevel 1 ( echo [!] Ошибка компиляции библиотеки & goto :fail )
lib /nologo /out:lib\Elements.lib build\el_*.obj
if errorlevel 1 ( echo [!] Ошибка сборки lib & goto :fail )

set "LIBS=lib\Elements.lib gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib"
echo === Сборка примеров ===
for %%f in (examples\*.cpp) do (
  echo   %%~nf
  cl /nologo /EHsc /std:c++17 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /I include /Fo:build\ /Fe:bin\%%~nf.exe %%f %LIBS% /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
  if errorlevel 1 ( echo [!] Ошибка сборки примера %%~nf & goto :fail )
)

echo.
echo Готово. Библиотека: lib\Elements.lib  (подключать include\elements.h)
echo Примеры в папке bin\
popd
endlocal
exit /b 0

:fail
popd
endlocal
exit /b 1
