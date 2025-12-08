# Assumes compiler is on your PATH (which it should be)

$AtmsIncludePath = Resolve-Path -Path "$PSScriptRoot\..\atmosphere"
$CommonArgs = "-I", $AtmsIncludePath, "-target", "spirv", "-profile", "spirv_1_5"

../../../slang/bin/slangc.exe "$PSScriptRoot\terrain_lighting.slang" @CommonArgs -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_terrain_draw  -capability SPV_EXT_shader_atomic_float_add -capability spvRayQueryKHR -o "$PSScriptRoot\cs_terrain_draw.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\terrain_prepass.slang" @CommonArgs -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -entry ts_control_main -entry ts_eval_main -o "$PSScriptRoot\terrain_prepass.spv"