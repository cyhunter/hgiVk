#ifndef PXR_IMAGING_HGIVK_OBJECT_H
#define PXR_IMAGING_HGIVK_OBJECT_H

#include "pxr/pxr.h"

PXR_NAMESPACE_OPEN_SCOPE


typedef enum HgiVkObjectType {
    HgiVkObjectTypeUnknown = 0,
    HgiVkObjectTypeTexture = 1,
    HgiVkObjectTypeBuffer = 2,
    HgiVkObjectTypeRenderPass = 3,
    HgiVkObjectTypePipeline = 4,
    HgiVkObjectTypeResourceBindings = 5,
    HgiVkObjectTypeShaderFunction = 6,
    HgiVkObjectTypeShaderProgram = 7,
    HgiVkObjectTypeSurface = 8,
    HgiVkObjectTypeSwapchain = 9,
};


/// HgiVkObject
///
/// Can store one pointer to any of the HgiVk classes.
/// 'type' is used to determine which class is stored.
///
struct HgiVkObject {
    HgiVkObjectType type;

    union {
        class HgiVkTexture* texture = nullptr;
        class HgiVkBuffer* buffer;
        class HgiVkRenderPass* renderPass;
        class HgiVkPipeline* pipeline;
        class HgiVkResourceBindings* resourceBindings;
        class HgiVkShaderFunction* shaderFunction;
        class HgiVkShaderProgram* shaderProgram;
        class HgiVkSurface* surface;
        class HgiVkSwapchain* swapchain;
    };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
