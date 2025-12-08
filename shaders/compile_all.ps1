$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$psFiles = Get-ChildItem -Path $scriptDir -Recurse -Filter *.ps1 |
           Where-Object { $_.DirectoryName -ne $scriptDir }

$total = $psFiles.Count
$index = 1

foreach ($file in $psFiles) {

    # colored output for the running message
    Write-Host "Running ($index/$total): $($file.Name)" -ForegroundColor Cyan

    try {
        Push-Location $PSScriptRoot
        & $file.FullName
        Pop-Location
    }
    catch {
        Write-Host "Compilation Failed: $($file.FullName): $_" -ForegroundColor Red
        exit 1
    }

    $index++
}
