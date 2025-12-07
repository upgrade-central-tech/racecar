$CommonArgs = "-target", "spirv", "-profile", "spirv_1_5", "-g2"

../../../slang/bin/slangc.exe "$PSScriptRoot\threshold.slang" @CommonArgs -fvk-use-entrypoint-name -entry threshold -o "$PSScriptRoot\threshold.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\downsample.slang" @CommonArgs -fvk-use-entrypoint-name -entry downsample -o "$PSScriptRoot\downsample.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\upsample.slang" @CommonArgs -fvk-use-entrypoint-name -entry upsample -o "$PSScriptRoot\upsample.spv"
