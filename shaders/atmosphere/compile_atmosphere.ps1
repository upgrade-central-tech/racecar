# Assumes compiler is on your PATH (which it should be)
slangc.exe "$PSScriptRoot\main.slang" -target spirv -g2 -profile spirv_1_4 -o "$PSScriptRoot\atmosphere.spv"