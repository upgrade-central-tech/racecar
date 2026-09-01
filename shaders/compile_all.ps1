param(
    [switch]$RayTracing
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$env:RACECAR_RAY_TRACING = if ($RayTracing) { '1' } else { '0' }

$rayTracingOnlyDirs = @('raytracing', 'reflections')

$psFiles = Get-ChildItem -Path $scriptDir -Recurse -Filter *.ps1 |
           Where-Object { $_.DirectoryName -ne $scriptDir }

$total = $psFiles.Count
$index = 1

Write-Host "Ray tracing: $(if ($RayTracing) { 'enabled' } else { 'disabled' })" -ForegroundColor Cyan

foreach ($file in $psFiles) {

    # colored output for the running message
    Write-Host "Running ($index/$total): $($file.Name)" -ForegroundColor Cyan

    $exitCode = 0

    try {
        Push-Location $PSScriptRoot
        $global:LASTEXITCODE = 0
        & $file.FullName
        $exitCode = $LASTEXITCODE
    }
    catch {
        Write-Host "Compilation Failed: $($file.FullName): $_" -ForegroundColor Red
        exit 1
    }
    finally {
        Pop-Location
    }

    if ($exitCode -ne 0) {
        Write-Host "Compilation Failed: $($file.FullName) (exit code $exitCode)" -ForegroundColor Red
        exit $exitCode
    }

    $index++
}
