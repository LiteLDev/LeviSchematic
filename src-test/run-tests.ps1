$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $projectRoot
try {
    $targets = @(
        'SessionGateTests', 'ChunkCpuTaskTests', 'SchematicRenderAlgorithmsTests',
        'NativeTextureVariantTests', 'RefactorStateTests', 'ProjectionStateTests', 'UiModelTests'
    )
    foreach ($target in $targets) {
        & xmake build $target
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $target" }
        & xmake run $target
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $target" }
    }
} finally {
    Pop-Location
}
