param(
    [Parameter(Mandatory=$true)][string]$ZigPath,
    [Parameter(Mandatory=$true)][string]$SdlDllDir,
    [string]$SdlHeaderDir = 'C:/devkitPro/portlibs/switch/include/SDL2'
)
$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
Push-Location $repoPath
try {
    New-Item -ItemType Directory -Force build/host-include/SDL2 | Out-Null
    Get-ChildItem -LiteralPath $SdlHeaderDir -Filter '*.h' -File | Copy-Item -Destination build/host-include/SDL2 -Force
    & $ZigPath c++ -target x86_64-windows-gnu -std=c++17 -Iinclude tests/model_tests.cpp source/model.cpp -o build/model_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Model test compilation failed' }
    & ./build/model_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Model tests failed' }
    $libraries = @('SDL2.dll','SDL2_ttf.dll','SDL2_image.dll','SDL2_mixer.dll') | ForEach-Object { Join-Path $SdlDllDir $_ }
    & $ZigPath c++ -target x86_64-windows-gnu -std=c++17 -Itests/stubs -Iinclude -Isource -Ibuild/host-include tests/desktop_preview.cpp source/ui.cpp source/input.cpp source/model.cpp @libraries -o build/desktop_preview.exe
    if ($LASTEXITCODE -ne 0) { throw 'Desktop preview compilation failed' }
    $previousPath = $env:PATH
    try {
        $env:PATH = $SdlDllDir + ';' + $env:PATH
        & ./build/desktop_preview.exe
        if ($LASTEXITCODE -ne 0) { throw 'Desktop workflow/render tests failed' }
    } finally { $env:PATH = $previousPath }
} finally { Pop-Location }
