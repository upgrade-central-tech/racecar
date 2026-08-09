# Assumes compiler is on your PATH (which it should be)

$AtmsIncludePath = Resolve-Path -Path "$PSScriptRoot\..\atmosphere"
$CommonArgs = "-I", $AtmsIncludePath, "-target", "spirv", "-profile", "spirv_1_5"

../../../slang/bin/slangc.exe "$PSScriptRoot\terrain_rt_displace.slang" @CommonArgs -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry cs_terrain_rt_displace -o "$PSScriptRoot\terrain_rt_displace.spv"
