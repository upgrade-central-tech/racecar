## Development

We use vcpkg for managing third-party C++ libraries, CMake for configuration, clang as the C++ compiler, and Ninja as our build system.

- For more information about vcpkg, see [vcpkg.io](https://vcpkg.io). Here's a tutorial on installing vcpkg with CMake and VS Code: [learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell).
- Get a CMake version above 4.0 at [cmake.org/download](https://cmake.org/download). Two CMake presets are provided, Debug and Release.
- Get the latest available LLVM release at [github.com/llvm/llvm-project/releases](https://github.com/llvm/llvm-project/releases) (for the clang compiler and other auxiliary tools).
- Get the latest Ninja binary at [github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases). Make sure it's available on your PATH.

### Vulkan SDK

Make sure your GPU supports Vulkan and your drivers are up to date. Get the [Vulkan SDK](https://vulkan.lunarg.com/) and run the installer with admin privileges so it can set the appropriate environment variables for you.

To check that Vulkan is ready for use, go to your Vulkan SDK directory (on Windows it's `C:/VulkanSDK/` unless otherwise specified) and run the `vkcube.exe` example within the `Bin` directory. If you see a rotating gray cube with the LunarG logo, then you are all set!
