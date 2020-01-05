#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/instance.h"
#include "pxr/imaging/hgiVk/surface.h"

PXR_NAMESPACE_OPEN_SCOPE

#if defined(__APPLE__) && defined(__OBJC__)
    #import <AppKit/AppKit.h>
    #import <Metal/Metal.h>
    #import <QuartzCore/QuartzCore.h>
#endif

static void
_CreateNativeSurface(
    VkInstance instance,
    HGI_NATIVE_WINDOW nativeWindow,
    HGI_NATIVE_PARENT nativeParent,
    VkSurfaceKHR* surfaceOut)
{
    VkSurfaceKHR surface = 0;

    #if defined(VK_USE_PLATFORM_WIN32_KHR)

        VkWin32SurfaceCreateInfoKHR createInfo =
            {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        createInfo.hinstance = nativeParent;
        createInfo.hwnd = nativeWindow;
        TF_VERIFY(
            vkCreateWin32SurfaceKHR(
                instance,
                &createInfo,
                HgiVkAllocator(),
                &surface) == VK_SUCCESS);

    #elif defined(VK_USE_PLATFORM_XLIB_KHR)

        VkXlibSurfaceCreateInfoKHR createInfo =
            {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
        createInfo.dpy = nativeParent;
        createInfo.window = nativeWindow;
        TF_VERIFY(
            vkCreateXlibSurfaceKHR(
                instance,
                &createInfo,
                HgiVkAllocator(),
                &surface) == VK_SUCCESS);

    #elif defined(VK_USE_PLATFORM_MACOS_MVK)

        #if defined(__APPLE__) && defined(__OBJC__)
            // iOS code see: https://github.com/KhronosGroup/MoltenVK/issues/78
            NSWindow* nsWindow = (NSWindow*) nativeWindow;
            NSView* view = [nsWindow contentView];
            // Need metal layer for the view. (or use: glfwCreateWindowSurface)
            assert([view isKindOfClass:[NSView class]]);
            if (![view.layer isKindOfClass:[CAMetalLayer class]]) {
                [view setLayer:[CAMetalLayer layer]];
                // [view setWantsLayer:YES];
            }

            VkMacOSSurfaceCreateInfoMVK createInfo =
                {VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK};
            createInfo.pNext = 0;
            createInfo.flags = 0;
            createInfo.pView = (void*)view;
            TF_VERIFY(
                vkCreateMacOSSurfaceMVK(
                    instance,
                    &createInfo,
                    HgiVkAllocator(),
                    &surface) == VK_SUCCESS);
        #endif

    #else
        #error Unsupported platform
    #endif

    *surfaceOut = surface;
}

HgiVkSurface::HgiVkSurface(
    HgiVkInstance* instance,
    HgiVkDevice* device,
    HgiVkSurfaceDesc const& desc)
    : _instance(instance)
    , _vkSurface(nullptr)
{
    _CreateNativeSurface(
        instance->GetVulkanInstance(),
        desc.window,
        desc.parent,
        &_vkSurface);
    TF_VERIFY(_vkSurface, "Invalid surface");

    VkBool32 presentSupported = 0;
    TF_VERIFY(
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device->GetVulkanPhysicalDevice(),
            device->GetVulkanDeviceQueueFamilyIndex(),
            _vkSurface,
            &presentSupported) == VK_SUCCESS
    );

    TF_VERIFY(presentSupported, "Presenting not supported on Vulkan device");
}

HgiVkSurface::~HgiVkSurface()
{
    vkDestroySurfaceKHR(
        _instance->GetVulkanInstance(),
        _vkSurface,
        HgiVkAllocator());
}

VkSurfaceKHR
HgiVkSurface::GetVulkanSurface() const
{
    return _vkSurface;
}

HgiVkSurfaceDesc::HgiVkSurfaceDesc()
    : window(0)
    , parent(0)
{
}

bool operator==(
    const HgiVkSurfaceDesc& lhs,
    const HgiVkSurfaceDesc& rhs)
{
    return lhs.window == rhs.window &&
           lhs.parent == rhs.parent;
}

bool operator!=(
    const HgiVkSurfaceDesc& lhs,
    const HgiVkSurfaceDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
