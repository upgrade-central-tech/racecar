# Assumes compiler is on your PATH (which it should be)

$IncludePath = Resolve-Path -Path "$PSScriptRoot\.."
$CommonArgs = "-I", $IncludePath, "-target", "spirv", "-profile", "spirv_1_5"

slangc.exe "$PSScriptRoot\atms_geo.slang" @CommonArgs -o "$PSScriptRoot\atms_geo.spv"