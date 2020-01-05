#ifndef PXR_IMAGING_HGIVK_SURFACE_H
#define PXR_IMAGING_HGIVK_SURFACE_H

#include <vector>

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE


#if defined(_WIN32)
    #include <windef.h>
    #define HGI_NATIVE_WINDOW HWND
    #define HGI_NATIVE_PARENT HMODULE
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #define HGI_NATIVE_WINDOW Window
    #define HGI_NATIVE_PARENT Display*
#elif defined(__APPLE__)
    #define HGI_NATIVE_WINDOW void*     /* id */
    #define HGI_NATIVE_PARENT uint32_t  /* CGDirectDisplayID */
#else
    #error Unsupported Platform
#endif


class HgiVkInstance;
class HgiVkDevice;
struct HgiVkSurfaceDesc;


///
/// \class HgiVkSurface
///
/// A Surface represents a native window for rendering into.
/// Only one Surface can be created per native window at a time.
///
class HgiVkSurface final {
public:
    HGIVK_API
    HgiVkSurface(
        HgiVkInstance* instance,
        HgiVkDevice* device,
        HgiVkSurfaceDesc const& desc);

    HGIVK_API
    virtual ~HgiVkSurface();

    /// Returns the vulkan surface (native-window)
    HGIVK_API
    VkSurfaceKHR GetVulkanSurface() const;

private:
    HgiVkSurface() = delete;
    HgiVkSurface & operator=(const HgiVkSurface&) = delete;
    HgiVkSurface(const HgiVkSurface&) = delete;

private:
    HgiVkInstance* _instance;
    VkSurfaceKHR _vkSurface;
};

typedef HgiVkSurface* HgiVkSurfaceHandle;
typedef std::vector<HgiVkSurfaceHandle> HgiVkSurfaceHandleVector;


/// \struct HgiVkSurfaceDesc
///
/// Describes the properties needed to create a GPU surface.
///
/// <ul>
/// <li>window:
///   Returns opaque struct to the platforms native window.</li>
/// <li>parent:
///   Returns the 'parent' of the platforms native window.
///   This is usually the 'display' or 'module'.</li>
/// </ul>
///
struct HgiVkSurfaceDesc {
    HGIVK_API
    HgiVkSurfaceDesc();

    HGI_NATIVE_WINDOW window;
    HGI_NATIVE_PARENT parent;
};

HGIVK_API
bool operator==(
    const HgiVkSurfaceDesc& lhs,
    const HgiVkSurfaceDesc& rhs);

HGIVK_API
bool operator!=(
    const HgiVkSurfaceDesc& lhs,
    const HgiVkSurfaceDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
