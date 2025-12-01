# Assumes compiler is on your PATH (which it should be)
../slang/bin/slangc.exe "$PSScriptRoot\prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\prepass.spv"
../slang/bin/slangc.exe "$PSScriptRoot\lighting.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\lighting.spv"
../slang/bin/slangc.exe "$PSScriptRoot\depth_prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\depth_prepass.spv"
../slang/bin/slangc.exe "$PSScriptRoot\pp_test.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry cs_pp_test -o "$PSScriptRoot\pp_test.spv"

../slang/bin/slangc.exe "$PSScriptRoot\terrain_prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -entry ts_control_main -entry ts_eval_main -o "$PSScriptRoot\terrain_prepass.spv"
