$CommonArgs = "-target", "spirv", "-profile", "spirv_1_5", "-g2"

../../../slang/bin/slangc.exe "$PSScriptRoot\brightness_threshold.slang" -target spirv -profile spirv_1_4 -g2 -fvk-use-entrypoint-name -entry cs_main -o "$PSScriptRoot\brightness_threshold.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\horz_blur.slang" -target spirv -profile spirv_1_4 -g2 -fvk-use-entrypoint-name -entry cs_main -o "$PSScriptRoot\horz_blur.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\vert_blur.slang" -target spirv -profile spirv_1_4 -g2 -fvk-use-entrypoint-name -entry cs_main -o "$PSScriptRoot\vert_blur.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\gather.slang" -target spirv -profile spirv_1_4 -g2 -fvk-use-entrypoint-name -entry cs_main -o "$PSScriptRoot\gather.spv"

../../../slang/bin/slangc.exe "$PSScriptRoot\downsample.slang" @CommonArgs -fvk-use-entrypoint-name -entry downsample -o "$PSScriptRoot\downsample.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\upsample.slang" @CommonArgs -fvk-use-entrypoint-name -entry upsample -o "$PSScriptRoot\upsample.spv"
