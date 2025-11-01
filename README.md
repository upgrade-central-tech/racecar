## Installing Vulkan

In order to run a Vulkan project, you first need to download and install the [Vulkan SDK](https://vulkan.lunarg.com/). Make sure to run the downloaded installed as administrator so that the installer can set the appropriate environment variables for you.

Once you have done this, you need to make sure your GPU driver supports Vulkan. Download and install a [Vulkan driver](https://developer.nvidia.com/vulkan-driver) from NVIDIA's website.

Finally, to check that Vulkan is ready for use, go to your Vulkan SDK directory (`C:/VulkanSDK/` unless otherwise specified) and run the `vkcube.exe` example within the `Bin` directory. If you see a rotating gray cube with the LunarG logo, then you are all set!
