set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgi)

pxr_library(hgi
    LIBRARIES
        gf
        tf

    PUBLIC_CLASSES
        blitEncoder
        buffer
        computeEncoder
        graphicsEncoder
        graphicsEncoderDesc
        hgi
        parallelGraphicsEncoder
        pipeline
        resourceBindings
        shaderFunction
        shaderProgram
        texture

    PUBLIC_HEADERS
        api.h
        blitEncoderOps.h
        encoderOps.h
        enums.h
        types.h
)
