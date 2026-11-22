$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
Push-Location $taskRoot
try {
    & cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build build --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
    & ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Audio tests failed. Installer was not created.' }
    $taskCompiler = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
    if (-not (Test-Path -LiteralPath $taskCompiler)) { throw 'Inno Setup 6 is required to create Setup.exe.' }
    $taskDist = Join-Path $taskRoot 'dist'
    New-Item -ItemType Directory -Force $taskDist | Out-Null
    & $taskCompiler "/DSourceRoot=$taskRoot" "/DOutputRoot=$taskDist" 'installer\windows.iss'
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
} finally { Pop-Location }
