#include "eglimagefactory.h"

#ifdef HAVE_LIBVA
#include <libavutil/hwcontext_vaapi.h>
#include <unistd.h>
#endif

#include <vector>

// Don't take a dependency on libdrm just for these constants
#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif
#ifndef fourcc_code
#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#endif
#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8 fourcc_code('R', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_GR88
#define DRM_FORMAT_GR88 fourcc_code('G', 'R', '8', '8')
#endif

EglImageFactory::EglImageFactory(IFFmpegRenderer* renderer) :
    m_Renderer(renderer),
    m_EGLExtDmaBuf(false),
    m_eglCreateImage(nullptr),
    m_eglDestroyImage(nullptr),
    m_eglCreateImageKHR(nullptr),
    m_eglDestroyImageKHR(nullptr),
    m_eglQueryDmaBufFormatsEXT(nullptr),
    m_eglQueryDmaBufModifiersEXT(nullptr)
{
}

bool EglImageFactory::initializeEGL(EGLDisplay,
                                    const EGLExtensions &ext)
{
    if (!ext.isSupported("EGL_EXT_image_dma_buf_import")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM-EGL: DMABUF unsupported");
        return false;
    }

    m_EGLExtDmaBuf = ext.isSupported("EGL_EXT_image_dma_buf_import_modifiers");
    if (m_EGLExtDmaBuf) {
        m_eglQueryDmaBufFormatsEXT = (typeof(m_eglQueryDmaBufFormatsEXT))eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        m_eglQueryDmaBufModifiersEXT = (typeof(m_eglQueryDmaBufModifiersEXT))eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    // NB: eglCreateImage() and eglCreateImageKHR() have slightly different definitions
    m_eglCreateImage = (typeof(m_eglCreateImage))eglGetProcAddress("eglCreateImage");
    m_eglCreateImageKHR = (typeof(m_eglCreateImageKHR))eglGetProcAddress("eglCreateImageKHR");
    m_eglDestroyImage = (typeof(m_eglDestroyImage))eglGetProcAddress("eglDestroyImage");
    m_eglDestroyImageKHR = (typeof(m_eglDestroyImageKHR))eglGetProcAddress("eglDestroyImageKHR");

    if (!(m_eglCreateImage && m_eglDestroyImage) &&
        !(m_eglCreateImageKHR && m_eglDestroyImageKHR)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Missing eglCreateImage()/eglDestroyImage() in EGL driver");
        return false;
    }

    return true;
}

void EglImageFactory::resetCache()
{
    // Cannot reset cache while in the middle of rendering
    SDL_assert(!m_LastImageCtx.has_value());
}

#ifdef HAVE_DRM

ssize_t EglImageFactory::exportDRMImages(AVFrame* frame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES])
{
    // freeEGLImages() must be called before exporting again
    SDL_assert(!m_LastImageCtx.has_value());

    SDL_assert(frame->format == AV_PIX_FMT_DRM_PRIME);
    AVDRMFrameDescriptor* drmFrame = (AVDRMFrameDescriptor*)frame->data[0];

    // DRM requires composed layers rather than separate layers per plane
    SDL_assert(drmFrame->nb_layers == 1);

    // Max 33 attributes (1 key + 1 value for each)
    const int MAX_ATTRIB_COUNT = 33 * 2;
    EGLAttrib attribs[MAX_ATTRIB_COUNT] = {
        EGL_LINUX_DRM_FOURCC_EXT, (EGLAttrib)drmFrame->layers[0].format,
        EGL_WIDTH, frame->width,
        EGL_HEIGHT, frame->height,
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
    };
    int attribIndex = 8;

    for (int i = 0; i < drmFrame->layers[0].nb_planes; ++i) {
        const auto &plane = drmFrame->layers[0].planes[i];
        const auto &object = drmFrame->objects[plane.object_index];

        switch (i) {
        case 0:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 1:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 2:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 3:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        default:
            Q_UNREACHABLE();
        }
    }

    // Add colorspace metadata
    switch (m_Renderer->getFrameColorspace(frame)) {
    case COLORSPACE_REC_601:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC601_EXT;
        break;
    case COLORSPACE_REC_709:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC709_EXT;
        break;
    case COLORSPACE_REC_2020:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC2020_EXT;
        break;
    }

    // Add color range metadata
    attribs[attribIndex++] = EGL_SAMPLE_RANGE_HINT_EXT;
    attribs[attribIndex++] = m_Renderer->isFrameFullRange(frame) ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT;

    // Add chroma siting metadata
    switch (frame->chroma_location) {
    case AVCHROMA_LOC_LEFT:
    case AVCHROMA_LOC_TOPLEFT:
        attribs[attribIndex++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
        attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_EXT;
        break;

    case AVCHROMA_LOC_CENTER:
    case AVCHROMA_LOC_TOP:
        attribs[attribIndex++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
        attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
        break;
    default:
        break;
    }
    switch (frame->chroma_location) {
    case AVCHROMA_LOC_TOPLEFT:
    case AVCHROMA_LOC_TOP:
        attribs[attribIndex++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
        attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_EXT;
        break;

    case AVCHROMA_LOC_LEFT:
    case AVCHROMA_LOC_CENTER:
        attribs[attribIndex++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
        attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
        break;
    default:
        break;
    }

    // Terminate the attribute list
    attribs[attribIndex++] = EGL_NONE;
    SDL_assert(attribIndex <= MAX_ATTRIB_COUNT);

    EglImageContext imgCtx(dpy, m_eglDestroyImage, m_eglDestroyImageKHR);

    // Our EGLImages are non-planar, so we only populate the first entry
    if (m_eglCreateImage) {
        imgCtx.images[0] = m_eglCreateImage(dpy, EGL_NO_CONTEXT,
                                            EGL_LINUX_DMA_BUF_EXT,
                                            nullptr, attribs);
        if (!imgCtx.images[0]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "eglCreateImage() Failed: %d", eglGetError());
            return -1;
        }
    }
    else {
        // Cast the EGLAttrib array elements to EGLint for the KHR extension
        EGLint intAttribs[MAX_ATTRIB_COUNT];
        for (int i = 0; i < MAX_ATTRIB_COUNT; i++) {
            intAttribs[i] = (EGLint)attribs[i];
        }

        imgCtx.images[0] = m_eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
                                               EGL_LINUX_DMA_BUF_EXT,
                                               nullptr, intAttribs);
        if (!imgCtx.images[0]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "eglCreateImageKHR() Failed: %d", eglGetError());
            return -1;
        }
    }

    imgCtx.count = 1;

    // Copy the output from the image context before we move it
    images[0] = imgCtx.images[0];

    // Store this image context
    m_LastImageCtx.emplace(std::move(imgCtx));

    return 1;
}

#endif

#ifdef HAVE_LIBVA

ssize_t EglImageFactory::exportVAImages(AVFrame *frame, uint32_t exportFlags, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES])
{
    // freeEGLImages() must be called before exporting again
    SDL_assert(!m_LastImageCtx.has_value());

    SDL_assert(frame->format == AV_PIX_FMT_VAAPI);
    auto hwFrameCtx = (AVHWFramesContext*)frame->hw_frames_ctx->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)hwFrameCtx->device_ctx->hwctx;
    VASurfaceID surface_id = (VASurfaceID)(uintptr_t)frame->data[3];

    // Sync the surface before doing anything. We need to do this even if we've got cached EGLImages.
    VAStatus st = vaSyncSurface(vaDeviceContext->display, surface_id);
    if (st != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vaSyncSurface() failed: %d", st);
        return -1;
    }

    EglImageContext imgCtx(dpy, m_eglDestroyImage, m_eglDestroyImageKHR);

    VADRMPRIMESurfaceDescriptor vaFrame;
    st = vaExportSurfaceHandle(vaDeviceContext->display,
                               surface_id,
                               VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                               exportFlags,
                               &vaFrame);
    if (st != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vaExportSurfaceHandle failed: %d", st);
        return -1;
    }

    SDL_assert(vaFrame.num_layers <= EGL_MAX_PLANES);

    for (size_t i = 0; i < vaFrame.num_layers; ++i) {
        const auto &layer = vaFrame.layers[i];

        // Max 33 attributes (1 key + 1 value for each)
        const int EGL_ATTRIB_COUNT = 33 * 2;
        EGLAttrib attribs[EGL_ATTRIB_COUNT] = {
            EGL_LINUX_DRM_FOURCC_EXT, (EGLAttrib)layer.drm_format,
            EGL_WIDTH, i == 0 ? frame->width : frame->width / 2,
            EGL_HEIGHT, i == 0 ? frame->height : frame->height / 2,
            EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        };

        int attribIndex = 8;
        for (size_t j = 0; j < layer.num_planes; j++) {
            const auto &object = vaFrame.objects[layer.object_index[j]];

            switch (j) {
            case 0:
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_FD_EXT;
                attribs[attribIndex++] = object.fd;
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
                attribs[attribIndex++] = layer.offset[0];
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
                attribs[attribIndex++] = layer.pitch[0];
                if (m_EGLExtDmaBuf) {
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier & 0xFFFFFFFF);
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier >> 32);
                }
                break;

            case 1:
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_FD_EXT;
                attribs[attribIndex++] = object.fd;
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
                attribs[attribIndex++] = layer.offset[1];
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
                attribs[attribIndex++] = layer.pitch[1];
                if (m_EGLExtDmaBuf) {
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier & 0xFFFFFFFF);
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier >> 32);
                }
                break;

            case 2:
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_FD_EXT;
                attribs[attribIndex++] = object.fd;
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
                attribs[attribIndex++] = layer.offset[2];
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
                attribs[attribIndex++] = layer.pitch[2];
                if (m_EGLExtDmaBuf) {
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier & 0xFFFFFFFF);
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier >> 32);
                }
                break;

            case 3:
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_FD_EXT;
                attribs[attribIndex++] = object.fd;
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
                attribs[attribIndex++] = layer.offset[3];
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
                attribs[attribIndex++] = layer.pitch[3];
                if (m_EGLExtDmaBuf) {
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier & 0xFFFFFFFF);
                    attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
                    attribs[attribIndex++] = (EGLint)(object.drm_format_modifier >> 32);
                }
                break;

            default:
                Q_UNREACHABLE();
            }
        }

        // For composed exports, add the YUV metadata
        if (vaFrame.num_layers == 1) {
            // Add colorspace metadata
            switch (m_Renderer->getFrameColorspace(frame)) {
            case COLORSPACE_REC_601:
                attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
                attribs[attribIndex++] = EGL_ITU_REC601_EXT;
                break;
            case COLORSPACE_REC_709:
                attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
                attribs[attribIndex++] = EGL_ITU_REC709_EXT;
                break;
            case COLORSPACE_REC_2020:
                attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
                attribs[attribIndex++] = EGL_ITU_REC2020_EXT;
                break;
            }

            // Add color range metadata
            attribs[attribIndex++] = EGL_SAMPLE_RANGE_HINT_EXT;
            attribs[attribIndex++] = m_Renderer->isFrameFullRange(frame) ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT;

            // Add chroma siting metadata
            switch (frame->chroma_location) {
            case AVCHROMA_LOC_LEFT:
            case AVCHROMA_LOC_TOPLEFT:
                attribs[attribIndex++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
                attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_EXT;
                break;

            case AVCHROMA_LOC_CENTER:
            case AVCHROMA_LOC_TOP:
                attribs[attribIndex++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
                attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
                break;
            default:
                break;
            }
            switch (frame->chroma_location) {
            case AVCHROMA_LOC_TOPLEFT:
            case AVCHROMA_LOC_TOP:
                attribs[attribIndex++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
                attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_EXT;
                break;

            case AVCHROMA_LOC_LEFT:
            case AVCHROMA_LOC_CENTER:
                attribs[attribIndex++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
                attribs[attribIndex++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
                break;
            default:
                break;
            }
        }

        // Terminate the attribute list
        attribs[attribIndex++] = EGL_NONE;
        SDL_assert(attribIndex <= EGL_ATTRIB_COUNT);

        if (m_eglCreateImage) {
            imgCtx.images[i] = m_eglCreateImage(dpy, EGL_NO_CONTEXT,
                                                EGL_LINUX_DMA_BUF_EXT,
                                                nullptr, attribs);
            if (!imgCtx.images[i]) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "eglCreateImage() Failed: %d", eglGetError());
                break;
            }
        }
        else {
            // Cast the EGLAttrib array elements to EGLint for the KHR extension
            EGLint intAttribs[EGL_ATTRIB_COUNT];
            for (int i = 0; i < attribIndex; i++) {
                intAttribs[i] = (EGLint)attribs[i];
            }

            imgCtx.images[i] = m_eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
                                                   EGL_LINUX_DMA_BUF_EXT,
                                                   nullptr, intAttribs);
            if (!imgCtx.images[i]) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "eglCreateImageKHR() Failed: %d", eglGetError());
                break;
            }
        }

        imgCtx.count++;
    }

    // Always close the exported FDs
    for (size_t i = 0; i < vaFrame.num_objects; ++i) {
        close(vaFrame.objects[i].fd);
    }

    // Check for failure
    if (vaFrame.num_layers != imgCtx.count) {
        return -1;
    }

    // Copy the output from the image context before we move it
    ssize_t count = imgCtx.count;
    memcpy(images, imgCtx.images, sizeof(EGLImage) * imgCtx.count);

    // Store this image context
    m_LastImageCtx.emplace(std::move(imgCtx));

    return count;
}

bool EglImageFactory::supportsImportingFormat(EGLDisplay dpy, EGLint format)
{
    if (!m_eglQueryDmaBufFormatsEXT) {
        // These are the standard formats used for importing separate layers of NV12.
        // We will assume all EGL implementations can handle these.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Assuming R8 and GR88 format support because eglQueryDmaBufFormatsEXT() is not supported");
        return format == DRM_FORMAT_R8 || format == DRM_FORMAT_GR88;
    }

    // Get the number of formats
    EGLint numFormats;
    if (!m_eglQueryDmaBufFormatsEXT(dpy, 0, nullptr, &numFormats)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "eglQueryDmaBufFormatsEXT() #1 failed: %d", eglGetError());
        return false;
    }
    else if (numFormats == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "eglQueryDmaBufFormatsEXT() returned no supported formats!");
        return false;
    }

    std::vector<EGLint> formats(numFormats);
    if (!m_eglQueryDmaBufFormatsEXT(dpy, numFormats, formats.data(), &numFormats)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "eglQueryDmaBufFormatsEXT() #2 failed: %d", eglGetError());
        return false;
    }

    for (EGLint i = 0; i < numFormats; i++) {
        if (format == formats[i]) {
            return true;
        }
    }

    return false;
}

bool EglImageFactory::supportsImportingModifier(EGLDisplay dpy, EGLint format, EGLuint64KHR modifier)
{
    // We assume linear and no modifiers are always supported
    if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
        return true;
    }

    if (!m_eglQueryDmaBufModifiersEXT) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Assuming linear modifier support because eglQueryDmaBufModifiersEXT() is not supported");
        return false;
    }

    // Get the number of modifiers
    EGLint numModifiers;
    if (!m_eglQueryDmaBufModifiersEXT(dpy, format, 0, nullptr, nullptr, &numModifiers)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "eglQueryDmaBufModifiersEXT() #1 failed: %d", eglGetError());
        return false;
    }
    else if (numModifiers == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "eglQueryDmaBufModifiersEXT() returned no supported modifiers!");
        return false;
    }

    std::vector<EGLuint64KHR> modifiers(numModifiers);
    if (!m_eglQueryDmaBufModifiersEXT(dpy, format, numModifiers, modifiers.data(), nullptr, &numModifiers)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "eglQueryDmaBufModifiersEXT() #2 failed: %d", eglGetError());
        return false;
    }

    for (EGLint i = 0; i < numModifiers; i++) {
        if (modifier == modifiers[i]) {
            return true;
        }
    }

    return false;
}

#endif

void EglImageFactory::freeEGLImages() {
    SDL_assert(m_LastImageCtx.has_value());
    m_LastImageCtx.reset();
}
