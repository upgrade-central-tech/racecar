$RayTracing = $env:RACECAR_RAY_TRACING -ne '0'

if ($RayTracing) {
    $RayTracingArgs = "-DRACECAR_RAY_TRACING=1", "-capability", "spvRayQueryKHR"
} else {
    $RayTracingArgs = , "-DRACECAR_RAY_TRACING=0"
}

../../../slang/bin/slangc.exe "$PSScriptRoot\transparency.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name @RayTracingArgs -entry vs_main -entry fs_main -o "$PSScriptRoot\transparency.spv"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }