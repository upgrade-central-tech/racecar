# Assumes compiler is on your PATH (which it should be)

$IncludePath = Resolve-Path -Path "$PSScriptRoot\.."
$CommonArgs = "-I", $IncludePath, "-target", "spirv", "-profile", "spirv_1_5"

slangc.exe "$PSScriptRoot\main.slang" @CommonArgs -o "$PSScriptRoot\atmosphere.spv"
slangc.exe "$PSScriptRoot\bake_atmosphere.slang" @CommonArgs -fvk-use-entrypoint-name -entry cs_bake_atmosphere -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\bake_atmosphere.spv"
slangc.exe "$PSScriptRoot\bake_atmosphere.slang" @CommonArgs -fvk-use-entrypoint-name -entry cs_bake_atmosphere_irradiance -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\bake_atmosphere_irradiance.spv"
slangc.exe "$PSScriptRoot\bake_atmosphere.slang" @CommonArgs -fvk-use-entrypoint-name -entry cs_bake_atmosphere_mips -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\bake_atmosphere_mips.spv"