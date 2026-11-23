# Elements: сборка всех вариантов библиотеки — статическая и динамическая (DLL)
# для x64 и x86. Результат в dist\<arch>\{static,dll}. Запуск:  & ".\build_all.ps1"
$ErrorActionPreference = 'Stop'
$proj = $PSScriptRoot

# найти vcvarsall.bat
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcall = $null
if (Test-Path $vswhere) {
    $vsroot = & $vswhere -latest -products * -property installationPath
    if ($vsroot) { $c = Join-Path $vsroot 'VC\Auxiliary\Build\vcvarsall.bat'; if (Test-Path $c) { $vcall = $c } }
}
if (-not $vcall) { $vcall = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat' }
if (-not (Test-Path $vcall)) { throw "vcvarsall.bat not found" }

# .def из экспортируемых имён заголовка (всегда в синхроне с API)
$hdr = Get-Content "$proj\include\elements.h" -Raw
$names = [regex]::Matches($hdr, '(?m)^\s*(?:void|int|el|const char \*)\s+(el_[a-z_]+)\s*\(') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$def = "LIBRARY Elements`r`nEXPORTS`r`n" + (($names | ForEach-Object { "  $_" }) -join "`r`n") + "`r`n"
[System.IO.File]::WriteAllText("$proj\elements.def", $def, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "exports: $($names.Count)"

function Build-Arch([string]$arch) {
    $obj = "$proj\build\$arch"; $out = "$proj\dist\$arch"
    New-Item -ItemType Directory -Force "$obj", "$out\static", "$out\dll" | Out-Null
    $src = (Get-ChildItem "$proj\src\*.cpp" | ForEach-Object { '"' + $_.FullName + '"' }) -join ' '
    $objs = '"' + $obj + '\el_*.obj"'
    $cflags = "/nologo /c /EHsc /std:c++17 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /I `"$proj\include`" /Fo:`"$obj\\`""
    $syslibs = 'gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib'
    $compile = "cl $cflags $src"
    $mklib = "lib /nologo /out:`"$out\static\Elements.lib`" $objs"
    $mkdll = "link /nologo /DLL /DEF:`"$proj\elements.def`" /OUT:`"$out\dll\Elements.dll`" /IMPLIB:`"$out\dll\Elements.lib`" $objs $syslibs"
    $full = "`"$vcall`" $arch >nul 2>&1 && $compile && $mklib && $mkdll"
    cmd /c $full
    if ($LASTEXITCODE -ne 0) { throw "build $arch failed" }
    Write-Host "[$arch] ok: dist\$arch\static\Elements.lib, dist\$arch\dll\Elements.dll"
}

Build-Arch 'x64'
Build-Arch 'x86'
Write-Host "Done. dist\x64 and dist\x86 (static + dll)."
