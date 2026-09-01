# Assumes compiler is on your PATH (which it should be)

$AtmsIncludePath = Resolve-Path -Path "$PSScriptRoot\..\atmosphere"
$CloudsIncludePath = Resolve-Path -Path "$PSScriptRoot\..\clouds"
$CommonArgs = "-I", $AtmsIncludePath, "-I", $CloudsIncludePath, "-target", "spirv", "-profile", "spirv_1_5"

../../../slang/bin/slangc.exe "$PSScriptRoot\sun_visibility.slang" @CommonArgs -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_sun_visibility -Wno-41012 -o "$PSScriptRoot\sun_visibility.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
