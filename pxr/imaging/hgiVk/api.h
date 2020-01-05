#ifndef PXR_IMAGING_HGIVK_API_H
#define PXR_IMAGING_HGIVK_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define HGIVK_API
#   define HGIVK_API_TEMPLATE_CLASS(...)
#   define HGIVK_API_TEMPLATE_STRUCT(...)
#   define HGIVK_LOCAL
#else
#   if defined(HGIVK_EXPORTS)
#       define HGIVK_API ARCH_EXPORT
#       define HGIVK_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define HGIVK_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define HGIVK_API ARCH_IMPORT
#       define HGIVK_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define HGIVK_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define HGIVK_LOCAL ARCH_HIDDEN
#endif

#endif

