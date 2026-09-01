# Assumes compiler is on your PATH (which it should be)

$AtmsIncludePath = Resolve-Path -Path "$PSScriptRoot\..\atmosphere"
$CommonArgs = "-I", $AtmsIncludePath, "-target", "spirv", "-profile", "spirv_1_5"

$RayTracing = $env:RACECAR_RAY_TRACING -ne '0'

if ($RayTracing) {
    $RayTracingArgs = @("-DRACECAR_RAY_TRACING=1"), "-capability", "spvRayQueryKHR"
} else {
    $RayTracingArgs = @("-DRACECAR_RAY_TRACING=0")
}

../../../slang/bin/slangc.exe "$PSScriptRoot\terrain_lighting.slang" @CommonArgs -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_terrain_draw -capability SPV_EXT_shader_atomic_float_add @RayTracingArgs -o "$PSScriptRoot\cs_terrain_draw.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
../../../slang/bin/slangc.exe "$PSScriptRoot\terrain_prepass.slang" @CommonArgs -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -entry ts_control_main -entry ts_eval_main -o "$PSScriptRoot\terrain_prepass.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }