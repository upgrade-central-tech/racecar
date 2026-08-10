$CommonArgs = "-target", "spirv", "-profile", "spirv_1_5", "-emit-spirv-directly", "-fvk-use-entrypoint-name"

../../../slang/bin/slangc.exe "$PSScriptRoot\reflection_mips.slang" @CommonArgs -entry cs_reflection_mips -o "$PSScriptRoot\reflection_mips.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\reflection_upsample.slang" @CommonArgs -entry cs_reflection_upsample -o "$PSScriptRoot\reflection_upsample.spv"
