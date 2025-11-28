# RACECAR: (Raceca)r (R)enderer

- Team 6: Charles Wang, Anthony Ge, Saahil Gupta, Aaron Jiang
- Pitch document: [docs.google.com/document/d/1j6_ybNivTQ-5yWpir-YyTrfw25QPabRPDMYnZbbpDFE](https://docs.google.com/document/d/1j6_ybNivTQ-5yWpir-YyTrfw25QPabRPDMYnZbbpDFE)
- Milestone 1 presentation: [docs.google.com/presentation/d/12kjNLzn8d41ME9kHvzYVEgyTugg4P2bqzO_xdeyU1to](https://docs.google.com/presentation/d/12kjNLzn8d41ME9kHvzYVEgyTugg4P2bqzO_xdeyU1to)

## Development

We use vcpkg for managing third-party C++ libraries, CMake for configuration, clang as the C++ compiler, and Ninja as our build system.

- For more information about vcpkg, see [vcpkg.io](https://vcpkg.io). Here's a tutorial on installing vcpkg with CMake and VS Code: [learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell).
- Get a CMake version above 4.0 at [cmake.org/download](https://cmake.org/download). Multiple CMake presets are available, targeting either debug or release builds.
- Get the latest available LLVM release at [github.com/llvm/llvm-project/releases](https://github.com/llvm/llvm-project/releases) (for the clang compiler and other tools). If using clangd, make sure you have version 21 or above.
- Get the latest Ninja binary at [github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases). Make sure it's available on your PATH.

### Vulkan SDK

#### Windows

Make sure your GPU supports Vulkan 1.4 and your drivers are up to date. Get the [Vulkan SDK](https://vulkan.lunarg.com/) and run the installer with admin privileges so it can set the appropriate environment variables for you.

To check that Vulkan is ready for use, go to your Vulkan SDK directory (by default should be `C:/VulkanSDK`) and run the `vkcube.exe` example within the `Bin` directory. If you see a rotating gray cube with the LunarG logo, then you are all set!

#### macOS

On Macs, developing for Vulkan is a bit more complicated. There is no direct Vulkan support; instead, [MoltenVK](https://github.com/KhronosGroup/MoltenVK) (which comes with the macOS Vulkan SDK) translates a subset of the Vulkan API into Metal.

macOS uses MoltenVK as the ICD, which sits between RACECAR and the actual Apple GPU drivers. Usually you would bundle `libvulkan.1.dylib`/`libMoltenVK.dylib` into your .app, but RACECAR is not being built like that. Unfortunately for us, by default SDL3 tries to find these libraries relative to the RACECAR binary, which... won't work.

This is why we have to explicitly call `SDL_Vulkan_LoadLibrary` in [sdl.cpp](src/sdl.cpp) with a hardcoded file path to the Vulkan dynamic library, because we can't rely on default behavior. For the developer this means two things:

- Install Vulkan SDK 1.4.328.0 or higher, which is the first version that includes MoltenVK 1.4 (aka Vulkan 1.4 support).
- Making the Vulkan SDK available globally on your system, meaning in `/usr/local`. This should have been an option during SDK setup, but the SDK also provides a `install_vulkan.py` script that does this post-installation.