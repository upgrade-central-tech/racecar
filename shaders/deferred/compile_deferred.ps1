# Assumes compiler is on your PATH (which it should be)

$RayTracing = $env:RACECAR_RAY_TRACING -ne '0'

if ($RayTracing) {
    $RayTracingArgs = "-DRACECAR_RAY_TRACING=1", "-capability", "spvRayQueryKHR"
} else {
    $RayTracingArgs = , "-DRACECAR_RAY_TRACING=0"
}

../../../slang/bin/slangc.exe  "$PSScriptRoot\prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\prepass.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
../../../slang/bin/slangc.exe  "$PSScriptRoot\lighting.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main @RayTracingArgs -o "$PSScriptRoot\lighting.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
../../../slang/bin/slangc.exe  "$PSScriptRoot\depth_prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\depth_prepass.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
../../../slang/bin/slangc.exe  "$PSScriptRoot\pp_test.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry cs_pp_test -o "$PSScriptRoot\pp_test.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }