#pragma once

#include "renderer.h"
#include "swframemapper.h"

#ifdef HAVE_EGL
#include "eglimagefactory.h"
#endif

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <set>
#include <unordered_map>
#include <mutex>

// This is only defined in Linux 6.8+ headers
#ifndef DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP
#define DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP	0x15
#endif

// Newer libdrm headers have these HDR structs, but some older ones don't.
namespace DrmDefs
{
    // HDR structs is copied from linux include/linux/hdmi.h
    struct hdr_metadata_infoframe
    {
        uint8_t eotf;
        uint8_t metadata_type;

        struct
        {
            uint16_t x, y;
        } display_primaries[3];

        struct
        {
            uint16_t x, y;
        } white_point;

        uint16_t max_display_mastering_luminance;
        uint16_t min_display_mastering_luminance;

        uint16_t max_cll;
        uint16_t max_fall;
    };

    struct hdr_output_metadata
    {
        uint32_t metadata_type;

        union {
            struct hdr_metadata_infoframe hdmi_metadata_type1;
        };
    };
}

class DrmRenderer : public IFFmpegRenderer {
    class DrmProperty {
    public:
        DrmProperty(uint32_t objectId, uint32_t objectType, drmModePropertyPtr prop, uint64_t initialValue) :
            m_ObjectId(objectId), m_ObjectType(objectType), m_Prop(prop), m_InitialValue(initialValue) {
            for (int i = 0; i < m_Prop->count_enums; i++) {
                m_Values.emplace(m_Prop->enums[i].name, m_Prop->enums[i].value);
            }
        }

        ~DrmProperty() noexcept {
            if (m_Prop) {
                drmModeFreeProperty(m_Prop);
            }
        }

        DrmProperty(DrmProperty &&other) = delete;
        DrmProperty(const DrmProperty &) = delete;

        bool isImmutable() const {
            return m_Prop->flags & DRM_MODE_PROP_IMMUTABLE;
        }

        const char* name() const {
            return m_Prop->name;
        }

        uint32_t id() const {
            return m_Prop->prop_id;
        }

        std::pair<uint64_t, uint64_t> range() const {
            if ((m_Prop->flags & (DRM_MODE_PROP_RANGE | DRM_MODE_PROP_SIGNED_RANGE)) &&
                m_Prop->count_values == 2) {
                return std::make_pair(m_Prop->values[0], m_Prop->values[1]);
            }
            else {
                SDL_assert(false);
                return std::make_pair(0, 0);
            }
        }

        bool containsValue(const std::string &name) const {
            return m_Values.find(name) != m_Values.end();
        }

        std::optional<uint64_t> value(const std::string &name) const {
            if (auto it = m_Values.find(name); it != m_Values.end()) {
                return it->second;
            }
            else {
                return std::nullopt;
            }
        }

        uint32_t objectId() const {
            return m_ObjectId;
        }

        uint32_t objectType() const {
            return m_ObjectType;
        }

        uint64_t initialValue() const {
            return m_InitialValue;
        }

    private:
        uint32_t m_ObjectId;
        uint32_t m_ObjectType;
        drmModePropertyPtr m_Prop;
        std::unordered_map<std::string, uint64_t> m_Values;
        uint64_t m_InitialValue;
    };

    class DrmPropertyMap {
    public:
        DrmPropertyMap() {}
        DrmPropertyMap(int fd, uint32_t objectId, uint32_t objectType) {
            SDL_assert(m_ObjectType == 0);
            SDL_assert(m_ObjectId == 0);
            load(fd, objectId, objectType);
        }

        DrmPropertyMap(const DrmPropertyMap &) = delete;
        DrmPropertyMap(DrmPropertyMap &&other) {
            m_Props = std::move(other.m_Props);
            m_ObjectId = other.m_ObjectId;
            m_ObjectType = other.m_ObjectType;
        }

        bool isValid() const {
            return m_ObjectType != 0;
        }

        void load(int fd, uint32_t objectId, uint32_t objectType) {
            drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, objectId, objectType);
            if (props) {
                for (uint32_t i = 0; i < props->count_props; i++) {
                    drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
                    if (prop) {
                        // DrmProperty takes ownership of the drmModePropertyPtr
                        m_Props.try_emplace(prop->name, objectId, objectType, prop, props->prop_values[i]);
                    }
                }

                drmModeFreeObjectProperties(props);
            }

            m_ObjectId = objectId;
            m_ObjectType = objectType;
        }

        bool hasProperty(const std::string& name) const {
            return m_Props.find(name) != m_Props.end();
        }

        const DrmProperty* property(const std::string& name) const {
            auto it = m_Props.find(name);
            if (it == m_Props.end()) {
                return nullptr;
            }

            return &it->second;
        }

        uint32_t objectId() const {
            return m_ObjectId;
        }

        uint32_t objectType() const {
            return m_ObjectType;
        }

    private:
        uint32_t m_ObjectId = 0;
        uint32_t m_ObjectType = 0;
        std::unordered_map<std::string, DrmProperty> m_Props;
    };

    class DrmPropertySetter {
        struct PlaneConfiguration {
            uint32_t crtcId;
            int32_t crtcX, crtcY;
            uint32_t crtcW, crtcH, srcX, srcY, srcW, srcH;
        };

        struct PlaneBuffer {
            uint32_t fbId;
            uint32_t dumbBufferHandle;
            AVFrame* frame;

            // Atomic only
            uint32_t pendingFbId;
            uint32_t pendingDumbBuffer;
            AVFrame* pendingFrame;
            bool modified;
        };

    public:
        DrmPropertySetter() {}
        ~DrmPropertySetter() {
            for (auto it = m_PlaneBuffers.begin(); it != m_PlaneBuffers.end(); it++) {
                SDL_assert(!it->second.fbId);
                SDL_assert(!it->second.dumbBufferHandle);

                if (it->second.pendingFbId) {
                    drmModeRmFB(m_Fd, it->second.pendingFbId);
                }
                if (it->second.pendingDumbBuffer) {
                    struct drm_mode_destroy_dumb destroyBuf = {};
                    destroyBuf.handle = it->second.pendingDumbBuffer;
                    drmIoctl(m_Fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
                }
            }

            if (m_AtomicReq) {
                drmModeAtomicFree(m_AtomicReq);
            }
        }

        DrmPropertySetter(const DrmPropertySetter &) = delete;
        DrmPropertySetter(DrmPropertySetter &&) = delete;

        void initialize(int drmFd, bool wantsAtomic, bool wantsAsyncFlip) {
            m_Fd = drmFd;
            m_Atomic = wantsAtomic && drmSetClientCap(drmFd, DRM_CLIENT_CAP_ATOMIC, 1) == 0;

            if (wantsAsyncFlip) {
                uint64_t val;
                if (!m_Atomic) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "V-sync cannot be disabled due to lack of atomic support");
                }
                else if (drmGetCap(m_Fd, DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &val) < 0 || !val) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "V-sync cannot be disabled due to lack of atomic async page flip support");
                }
                else {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Using atomic async page flips with V-sync disabled");
                    m_AsyncFlip = true;
                }
            }
        }

        bool set(const DrmProperty& prop, uint64_t value, bool verbose = true) {
            int err;

            if (m_Atomic) {
                // Synchronize with other threads that might be committing or setting properties
                std::lock_guard lg { m_Lock };

                if (!m_AtomicReq) {
                    m_AtomicReq = drmModeAtomicAlloc();
                }

                err = drmModeAtomicAddProperty(m_AtomicReq, prop.objectId(), prop.id(), value);

                // This returns the new count of properties on success,
                // so normalize it to 0 like drmModeObjectSetProperty()
                if (err > 0) {
                    err = 0;
                }
            }
            else {
                err = drmModeObjectSetProperty(m_Fd, prop.objectId(), prop.objectType(), prop.id(), value);
            }

            if (verbose && err == 0) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Set property '%s': %" PRIu64,
                            prop.name(),
                            value);
            }
            else if (err < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to set property '%s': %d",
                             prop.name(),
                             err);
            }

            return err == 0;
        }

        bool set(const DrmProperty& prop, const std::string &value) {
            std::optional<uint64_t> propValue = prop.value(value);
            if (!propValue) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Property '%s' has no supported enum value '%s'",
                             prop.name(),
                             value.c_str());
                return false;
            }

            if (set(prop, *propValue, false)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Set property '%s': %s",
                            prop.name(),
                            value.c_str());
                return true;
            }
            else {
                return false;
            }
        }

        // Unconditionally takes ownership of fbId and dumbBufferHandle (if present)
        // If provided, frame is referenced to keep the FB's backing DMA-BUFs around
        bool flipPlane(const DrmPropertyMap& plane, uint32_t fbId, uint32_t dumbBufferHandle, AVFrame* frame) {
            bool ret;

            // If a frame was provided, clone a reference to it to hold until the next flip
            if (frame) {
                frame = av_frame_clone(frame);
            }

            if (m_Atomic) {
                std::lock_guard lg { m_Lock };

                ret = set(*plane.property("FB_ID"), fbId, false);
                if (ret) {
                    // If we updated the FB_ID property, free the old pending buffer.
                    // Otherwise, we'll free the new buffer which was never used.
                    auto &pb = m_PlaneBuffers[plane.objectId()];
                    std::swap(fbId, pb.pendingFbId);
                    std::swap(dumbBufferHandle, pb.pendingDumbBuffer);
                    std::swap(frame, pb.pendingFrame);
                    pb.modified = true;
                }
            }
            else {
                PlaneConfiguration planeConfig;

                {
                    // Latch the plane configuration and release the lock
                    std::lock_guard lg { m_Lock };
                    planeConfig = m_PlaneConfigs.at(plane.objectId());
                }

                int err = drmModeSetPlane(m_Fd, plane.objectId(),
                                          planeConfig.crtcId, fbId, 0,
                                          planeConfig.crtcX, planeConfig.crtcY,
                                          planeConfig.crtcW, planeConfig.crtcH,
                                          planeConfig.srcX, planeConfig.srcY,
                                          planeConfig.srcW, planeConfig.srcH);

                // If we succeeded updating the plane, free the old FB state
                // Otherwise, we'll free the new data which was never used.
                if (err == 0) {
                    std::lock_guard lg { m_Lock };

                    auto &pb = m_PlaneBuffers[plane.objectId()];
                    std::swap(fbId, pb.fbId);
                    std::swap(dumbBufferHandle, pb.dumbBufferHandle);
                    std::swap(frame, pb.frame);

                    ret = true;
                }
                else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "drmModeSetPlane() failed: %d",
                                 err);
                    ret = false;
                }
            }

            // Free the unused resources
            if (fbId) {
                drmModeRmFB(m_Fd, fbId);
            }
            if (dumbBufferHandle) {
                struct drm_mode_destroy_dumb destroyBuf = {};
                destroyBuf.handle = dumbBufferHandle;
                drmIoctl(m_Fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
            }
            av_frame_free(&frame);

            return ret;
        }

        bool configurePlane(const DrmPropertyMap& plane,
                            uint32_t crtcId,
                            int32_t crtcX, int32_t crtcY,
                            uint32_t crtcW, uint32_t crtcH,
                            uint32_t srcX, uint32_t srcY,
                            uint32_t srcW, uint32_t srcH) {
            bool ret = true;
            std::lock_guard lg { m_Lock };

            if (m_Atomic) {
                ret = ret && set(*plane.property("CRTC_ID"), crtcId, false);
                ret = ret && set(*plane.property("CRTC_X"), crtcX, false);
                ret = ret && set(*plane.property("CRTC_Y"), crtcY, false);
                ret = ret && set(*plane.property("CRTC_W"), crtcW, false);
                ret = ret && set(*plane.property("CRTC_H"), crtcH, false);
                ret = ret && set(*plane.property("SRC_X"), srcX, false);
                ret = ret && set(*plane.property("SRC_Y"), srcY, false);
                ret = ret && set(*plane.property("SRC_W"), srcW, false);
                ret = ret && set(*plane.property("SRC_H"), srcH, false);
            }
            else {
                auto& planeConfig = m_PlaneConfigs[plane.objectId()];
                planeConfig.crtcId = crtcId;
                planeConfig.crtcX = crtcX;
                planeConfig.crtcY = crtcY;
                planeConfig.crtcW = crtcW;
                planeConfig.crtcH = crtcH;
                planeConfig.srcX = srcX;
                planeConfig.srcY = srcY;
                planeConfig.srcW = srcW;
                planeConfig.srcH = srcH;
            }

            return ret;
        }

        void disablePlane(const DrmPropertyMap& plane) {
            if (plane.isValid()) {
                configurePlane(plane, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                flipPlane(plane, 0, 0, nullptr);
            }
        }

        bool apply() {
            if (!m_Atomic) {
                return 0;
            }

            drmModeAtomicReqPtr req = nullptr;
            std::unordered_map<uint32_t, PlaneBuffer> pendingBuffers;

            {
                // Take ownership of the current atomic request to commit it and
                // allow other threads to queue up changes for the next one.
                std::lock_guard lg { m_Lock };
                std::swap(req, m_AtomicReq);
                std::swap(pendingBuffers, m_PlaneBuffers);
            }

            if (!req) {
                // Nothing to apply
                return true;
            }

            // Try an async flip if requested
            int err = drmModeAtomicCommit(m_Fd, req,
                                          m_AsyncFlip ? DRM_MODE_PAGE_FLIP_ASYNC : DRM_MODE_ATOMIC_ALLOW_MODESET,
                                          nullptr);

            // The driver may not support async flips (especially if we changed a non-FB_ID property),
            // so try again with a regular flip if we get an error from the async flip attempt.
            //
            // We pass DRM_MODE_ATOMIC_ALLOW_MODESET because changing HDR state may require a modeset.
            if (err < 0 && m_AsyncFlip) {
                err = drmModeAtomicCommit(m_Fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
            }
            if (err < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "drmModeAtomicCommit() failed: %d",
                             err);
            }

            // Update the buffer state for any modified planes
            std::lock_guard lg { m_Lock };
            for (auto it = pendingBuffers.begin(); it != pendingBuffers.end(); it++) {
                if (err == 0 && it->second.modified) {
                    if (it->second.fbId) {
                        drmModeRmFB(m_Fd, it->second.fbId);
                        it->second.fbId = 0;
                    }
                    if (it->second.dumbBufferHandle) {
                        struct drm_mode_destroy_dumb destroyBuf = {};
                        destroyBuf.handle = it->second.dumbBufferHandle;
                        drmIoctl(m_Fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
                        it->second.dumbBufferHandle = 0;
                    }
                    av_frame_free(&it->second.frame);

                    // The pending buffers become the active buffers for this FB
                    auto &pb = m_PlaneBuffers[it->first];
                    pb.fbId = it->second.pendingFbId;
                    pb.dumbBufferHandle = it->second.pendingDumbBuffer;
                    pb.frame = it->second.pendingFrame;
                }
                else if (err < 0 || it->second.fbId || it->second.dumbBufferHandle || it->second.frame) {
                    // Free the old pending buffers on a failed commit
                    if (it->second.pendingFbId) {
                        SDL_assert(err < 0);
                        drmModeRmFB(m_Fd, it->second.pendingFbId);
                    }
                    if (it->second.pendingDumbBuffer) {
                        SDL_assert(err < 0);
                        struct drm_mode_destroy_dumb destroyBuf = {};
                        destroyBuf.handle = it->second.pendingDumbBuffer;
                        drmIoctl(m_Fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
                    }
                    av_frame_free(&it->second.pendingFrame);

                    // This FB wasn't modified in this commit, so the current buffers stay around
                    auto &pb = m_PlaneBuffers[it->first];
                    pb.fbId = it->second.fbId;
                    pb.dumbBufferHandle = it->second.dumbBufferHandle;
                    pb.frame = it->second.frame;
                }

                // NB: We swapped in a new plane buffers map which will clear the modified value.
                // It's important that we don't try to clear it here because we might stomp on
                // a flipPlane() performed by another thread that queued up another modification.
            }

            drmModeAtomicFree(req);
            return err == 0;
        }

        bool isAtomic() {
            return m_Atomic;
        }

    private:
        int m_Fd = -1;
        bool m_Atomic = false;
        bool m_AsyncFlip = false;
        std::recursive_mutex m_Lock;
        std::unordered_map<uint32_t, PlaneBuffer> m_PlaneBuffers;

        // Legacy context
        std::unordered_map<uint32_t, PlaneConfiguration> m_PlaneConfigs;

        // Atomic context
        drmModeAtomicReqPtr m_AtomicReq = nullptr;
    };

public:
    DrmRenderer(AVHWDeviceType hwDeviceType = AV_HWDEVICE_TYPE_NONE, IFFmpegRenderer *backendRenderer = nullptr);
    virtual ~DrmRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual bool prepareDecoderContextInGetFormat(AVCodecContext*, AVPixelFormat) override;
    virtual void prepareToRender() override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual enum AVPixelFormat getPreferredPixelFormat(int videoFormat) override;
    virtual bool isPixelFormatSupported(int videoFormat, AVPixelFormat pixelFormat) override;
    virtual int getRendererAttributes() override;
    virtual bool testRenderFrame(AVFrame* frame) override;
    virtual bool isDirectRenderingSupported() override;
    virtual int getDecoderColorspace() override;
    virtual void setHdrMode(bool enabled) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType type) override;
#ifdef HAVE_EGL
    virtual bool canExportEGL() override;
    virtual AVPixelFormat getEGLImagePixelFormat() override;
    virtual bool initializeEGL(EGLDisplay dpy, const EGLExtensions &ext) override;
    virtual ssize_t exportEGLImages(AVFrame *frame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) override;
#endif

private:
    const char* getDrmColorEncodingValue(AVFrame* frame);
    const char* getDrmColorRangeValue(AVFrame* frame);
    bool mapSoftwareFrame(AVFrame* frame, AVDRMFrameDescriptor* mappedFrame);
    bool addFbForFrame(AVFrame* frame, uint32_t* newFbId, bool testMode);
    bool uploadSurfaceToFb(SDL_Surface *surface, uint32_t* handle, uint32_t* fbId);
    static bool drmFormatMatchesVideoFormat(uint32_t drmFormat, int videoFormat);

    IFFmpegRenderer* m_BackendRenderer;
    SDL_Window* m_Window;
    bool m_DrmPrimeBackend;
    AVHWDeviceType m_HwDeviceType;
    AVBufferRef* m_HwContext;
    int m_DrmFd;
    bool m_DrmIsMaster;
    bool m_DrmStateModified;
    bool m_DrmSupportsModifiers;
    bool m_MustCloseDrmFd;
    bool m_SupportsDirectRendering;
    int m_VideoFormat;
    DrmPropertyMap m_Encoder;
    DrmPropertyMap m_Connector;
    DrmPropertyMap m_Crtc;
    DrmPropertyMap m_VideoPlane;
    DrmPropertyMap m_OverlayPlanes[Overlay::OverlayMax];
    DrmPropertySetter m_PropSetter;
    SDL_Rect m_OverlayRects[Overlay::OverlayMax];
    drmVersionPtr m_Version;
    uint32_t m_HdrOutputMetadataBlobId;
    SDL_Rect m_OutputRect;
    std::set<uint32_t> m_SupportedVideoPlaneFormats;

    static constexpr int k_SwFrameCount = 2;
    SwFrameMapper m_SwFrameMapper;
    int m_CurrentSwFrameIdx;
    struct {
        uint32_t handle;
        uint32_t pitch;
        uint64_t size;
        uint8_t* mapping;
        int primeFd;
    } m_SwFrame[k_SwFrameCount];

#ifdef HAVE_EGL
    EglImageFactory m_EglImageFactory;
#endif
};

