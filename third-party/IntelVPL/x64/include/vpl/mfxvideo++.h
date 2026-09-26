/*###########################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ###########################################################################*/

#ifndef __MFXVIDEOPLUSPLUS_H
#define __MFXVIDEOPLUSPLUS_H

#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
#include "vpl/mfx.h"
#define MFX_IMPL_ACCELMODE(x) (0xff00 & (x))
#else
#include "mfxvideo.h"
#endif

class MFXVideoSessionBase {
public:
    virtual ~MFXVideoSessionBase() {}

    virtual mfxStatus Init(mfxIMPL impl, mfxVersion* ver) = 0;
    virtual mfxStatus InitEx(mfxInitParam par) = 0;
    virtual mfxStatus Close(void) = 0;

    virtual mfxStatus QueryIMPL(mfxIMPL* impl) = 0;
    virtual mfxStatus QueryVersion(mfxVersion* version) = 0;

    virtual mfxStatus JoinSession(mfxSession child_session) = 0;
    virtual mfxStatus DisjoinSession() = 0;
    virtual mfxStatus CloneSession(mfxSession* clone) = 0;
    virtual mfxStatus SetPriority(mfxPriority priority) = 0;
    virtual mfxStatus GetPriority(mfxPriority* priority) = 0;

    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator* allocator) = 0;
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) = 0;
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL* hdl) = 0;
    virtual mfxStatus QueryPlatform(mfxPlatform* platform) = 0;

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) = 0;

    virtual mfxStatus GetSurfaceForEncode(mfxFrameSurface1** output_surf) = 0;
    virtual mfxStatus GetSurfaceForDecode(mfxFrameSurface1** output_surf) = 0;
    virtual mfxStatus GetSurfaceForVPP(mfxFrameSurface1** output_surf) = 0;
    virtual mfxStatus GetSurfaceForVPPOut(mfxFrameSurface1** output_surf) = 0;

    virtual operator mfxSession(void) = 0;
};

class MFXVideoENCODEBase {
public:
    virtual ~MFXVideoENCODEBase() {}

    virtual mfxStatus Query(mfxVideoParam* in, mfxVideoParam* out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam* par, mfxFrameAllocRequest* request) = 0;
    virtual mfxStatus Init(mfxVideoParam* par) = 0;
    virtual mfxStatus Reset(mfxVideoParam* par) = 0;
    virtual mfxStatus Close(void) = 0;

    virtual mfxStatus GetVideoParam(mfxVideoParam* par) = 0;
    virtual mfxStatus GetEncodeStat(mfxEncodeStat* stat) = 0;

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) = 0;

    virtual mfxStatus GetSurface(mfxFrameSurface1** output_surf) = 0;
};

class MFXVideoDECODEBase {
public:
    virtual ~MFXVideoDECODEBase() {}

    virtual mfxStatus Query(mfxVideoParam* in, mfxVideoParam* out) = 0;
    virtual mfxStatus DecodeHeader(mfxBitstream* bs, mfxVideoParam* par) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam* par, mfxFrameAllocRequest* request) = 0;
    virtual mfxStatus Init(mfxVideoParam* par) = 0;
    virtual mfxStatus Reset(mfxVideoParam* par) = 0;
    virtual mfxStatus Close(void) = 0;

    virtual mfxStatus GetVideoParam(mfxVideoParam* par) = 0;

    virtual mfxStatus GetDecodeStat(mfxDecodeStat* stat) = 0;
    virtual mfxStatus GetPayload(mfxU64* ts, mfxPayload* payload) = 0;
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) = 0;
    virtual mfxStatus DecodeFrameAsync(mfxBitstream* bs,
        mfxFrameSurface1* surface_work,
        mfxFrameSurface1** surface_out,
        mfxSyncPoint* syncp) = 0;

    virtual mfxStatus GetSurface(mfxFrameSurface1** output_surf) = 0;
};

class MFXVideoVPPBase {
public:
    virtual ~MFXVideoVPPBase() {}
    virtual mfxStatus Query(mfxVideoParam* in, mfxVideoParam* out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam* par, mfxFrameAllocRequest request[2]) = 0;
    virtual mfxStatus Init(mfxVideoParam* par) = 0;
    virtual mfxStatus Reset(mfxVideoParam* par) = 0;
    virtual mfxStatus Close(void) = 0;

    virtual mfxStatus GetVideoParam(mfxVideoParam* par) = 0;
    virtual mfxStatus GetVPPStat(mfxVPPStat* stat) = 0;
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1* in,
        mfxFrameSurface1* out,
        mfxExtVppAuxData* aux,
        mfxSyncPoint* syncp) = 0;

    virtual mfxStatus GetSurfaceIn(mfxFrameSurface1** output_surf) = 0;
    virtual mfxStatus GetSurfaceOut(mfxFrameSurface1** output_surf) = 0;
    virtual mfxStatus ProcessFrameAsync(mfxFrameSurface1* in, mfxFrameSurface1** out) = 0;
};

class MFXVideoSession : public MFXVideoSessionBase {
public:
    MFXVideoSession(void) {
        m_session     = (mfxSession)0;
#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
        m_loader      = (mfxLoader)0;
#endif
    }
    virtual ~MFXVideoSession(void) {
        Close();
    }

    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver) override {
#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
        mfxInitParam par   = {};
        par.Implementation = impl;
        par.Version        = *ver;
        return InitSession(par);
#else
        return MFXInit(impl, ver, &m_session);
#endif
    }
    virtual mfxStatus InitEx(mfxInitParam par) override {
#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
        return InitSession(par);
#else
        return MFXInitEx(par, &m_session);
#endif
    }
    virtual mfxStatus Close(void) override {
#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
        if (m_session) {
            mfxStatus mfxRes;
            mfxRes    = MFXClose(m_session);
            m_session = (mfxSession)0;
            if (m_loader) {
                MFXUnload(m_loader);
                m_loader = (mfxLoader)0;
            }
            return mfxRes;
        }
        else {
            return MFX_ERR_NONE;
        }
#else
        mfxStatus mfxRes;
        mfxRes    = MFXClose(m_session);
        m_session = (mfxSession)0;
        return mfxRes;
#endif
    }

    virtual mfxStatus QueryIMPL(mfxIMPL *impl) override {
        return MFXQueryIMPL(m_session, impl);
    }
    virtual mfxStatus QueryVersion(mfxVersion *version) override {
        return MFXQueryVersion(m_session, version);
    }

    virtual mfxStatus JoinSession(mfxSession child_session) override {
        return MFXJoinSession(m_session, child_session);
    }
    virtual mfxStatus DisjoinSession() override {
        return MFXDisjoinSession(m_session);
    }
    virtual mfxStatus CloneSession(mfxSession *clone) override {
        return MFXCloneSession(m_session, clone);
    }
    virtual mfxStatus SetPriority(mfxPriority priority) override {
        return MFXSetPriority(m_session, priority);
    }
    virtual mfxStatus GetPriority(mfxPriority *priority) override {
        return MFXGetPriority(m_session, priority);
    }

    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator) override {
        return MFXVideoCORE_SetFrameAllocator(m_session, allocator);
    }
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) override {
        return MFXVideoCORE_SetHandle(m_session, type, hdl);
    }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *hdl) override {
        return MFXVideoCORE_GetHandle(m_session, type, hdl);
    }
    virtual mfxStatus QueryPlatform(mfxPlatform *platform) override {
        return MFXVideoCORE_QueryPlatform(m_session, platform);
    }

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) override {
        return MFXVideoCORE_SyncOperation(m_session, syncp, wait);
    }

    virtual mfxStatus GetSurfaceForEncode(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForEncode(m_session, output_surf);
    }
    virtual mfxStatus GetSurfaceForDecode(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForDecode(m_session, output_surf);
    }
    virtual mfxStatus GetSurfaceForVPP   (mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForVPP   (m_session, output_surf);
    }
    virtual mfxStatus GetSurfaceForVPPOut(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForVPPOut(m_session, output_surf);
    }

    virtual operator mfxSession(void) override {
        return m_session;
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session

#ifdef MFXVIDEO_CPP_ENABLE_MFXLOAD
    mfxLoader m_loader;

    inline void InitVariant(mfxVariant *var, mfxU32 data) {
        var->Version.Version = (mfxU16)MFX_VARIANT_VERSION;
        var->Type            = MFX_VARIANT_TYPE_U32;
        var->Data.U32        = data;
    }

    inline void InitVariant(mfxVariant *var, mfxU16 data) {
        var->Version.Version = (mfxU16)MFX_VARIANT_VERSION;
        var->Type            = MFX_VARIANT_TYPE_U16;
        var->Data.U16        = data;
    }

    inline void InitVariant(mfxVariant *var, mfxHDL data) {
        var->Version.Version = (mfxU16)MFX_VARIANT_VERSION;
        var->Type            = MFX_VARIANT_TYPE_PTR;
        var->Data.Ptr        = data;
    }

    template <typename varDataType>
    mfxStatus CreateConfig(varDataType data, const char *propertyName) {
        mfxConfig cfg = MFXCreateConfig(m_loader);
        if (cfg == nullptr)
            return MFX_ERR_NULL_PTR;

        mfxVariant variant;
        InitVariant(&variant, data);

        return MFXSetConfigFilterProperty(cfg, (mfxU8 *)propertyName, variant);
    }

    mfxStatus InitSession(mfxInitParam par) {
        // already initialized
        if (m_session)
            return MFX_ERR_NONE;

        m_loader = MFXLoad();
        if (!m_loader)
            return MFX_ERR_NOT_FOUND;

        mfxStatus mfxRes = MFX_ERR_NONE;

        mfxU32 implBaseType = MFX_IMPL_BASETYPE(par.Implementation);

        // select implementation type
        switch (implBaseType) {
            case MFX_IMPL_AUTO:
            case MFX_IMPL_AUTO_ANY:
                break;

            case MFX_IMPL_SOFTWARE:
                mfxRes = CreateConfig<mfxU32>(MFX_IMPL_TYPE_SOFTWARE, "mfxImplDescription.Impl");
                break;

            case MFX_IMPL_HARDWARE:
            case MFX_IMPL_HARDWARE_ANY:
            case MFX_IMPL_HARDWARE2:
            case MFX_IMPL_HARDWARE3:
            case MFX_IMPL_HARDWARE4:
                mfxRes = CreateConfig<mfxU32>(MFX_IMPL_TYPE_HARDWARE, "mfxImplDescription.Impl");
                break;

            default:
                mfxRes = MFX_ERR_UNSUPPORTED;
                break;
        }

        if (MFX_ERR_NONE != mfxRes)
            return MFX_ERR_UNSUPPORTED;

        // select adapter index (if specified)
        // see notes below about how VendorImplID is interpreted for each acceleration mode
        switch (implBaseType) {
            case MFX_IMPL_HARDWARE:
                mfxRes = CreateConfig<mfxU32>(0, "mfxImplDescription.VendorImplID");
                break;
            case MFX_IMPL_HARDWARE2:
                mfxRes = CreateConfig<mfxU32>(1, "mfxImplDescription.VendorImplID");
                break;
            case MFX_IMPL_HARDWARE3:
                mfxRes = CreateConfig<mfxU32>(2, "mfxImplDescription.VendorImplID");
                break;
            case MFX_IMPL_HARDWARE4:
                mfxRes = CreateConfig<mfxU32>(3, "mfxImplDescription.VendorImplID");
                break;
        }

        if (MFX_ERR_NONE != mfxRes)
            return MFX_ERR_UNSUPPORTED;

        mfxU32 implAccelMode = MFX_IMPL_ACCELMODE(par.Implementation);
        if (implAccelMode == MFX_IMPL_VIA_D3D9) {
            // D3D9 - because VendorImplID corresponds to DXGI adapter index (DX11 enumeration),
            //   this may not map directly to D3D9 index in multi-adapter/multi-monitor configurations
            mfxRes = CreateConfig<mfxU32>(MFX_ACCEL_MODE_VIA_D3D9,
                                        "mfxImplDescription.AccelerationMode");
        }
        else if (implAccelMode == MFX_IMPL_VIA_D3D11) {
            // D3D11 - VendorImplID corresponds to DXGI adapter index
            mfxRes = CreateConfig<mfxU32>(MFX_ACCEL_MODE_VIA_D3D11,
                                        "mfxImplDescription.AccelerationMode");
        }
        else if (implAccelMode == MFX_IMPL_VIA_VAAPI) {
            // VAAPI - in general MFXInitEx treats any HARDWAREn the same way (relies on application to pass
            //   correct VADisplay via SetHandle), but 2.x RT only reports actual number of adapters
            mfxRes = CreateConfig<mfxU32>(MFX_ACCEL_MODE_VIA_VAAPI,
                                        "mfxImplDescription.AccelerationMode");
        }

        if (MFX_ERR_NONE != mfxRes)
            return MFX_ERR_UNSUPPORTED;

        // set required API level
        mfxRes =
            CreateConfig<mfxU32>(par.Version.Version, "mfxImplDescription.ApiVersion.Version");
        if (MFX_ERR_NONE != mfxRes)
            return MFX_ERR_UNSUPPORTED;

        // set GPUCopy parameter
        if (par.GPUCopy) {
            mfxRes = CreateConfig<mfxU16>(par.GPUCopy, "DeviceCopy");
            if (MFX_ERR_NONE != mfxRes)
                return MFX_ERR_UNSUPPORTED;
        }

        // ExternalThreads was deprecated in API 2.x along with MFXDoWork()
        if (par.ExternalThreads) {
            return MFX_ERR_UNSUPPORTED;
        }

        // pass extBufs
        if (par.NumExtParam) {
            for (mfxU32 idx = 0; idx < par.NumExtParam; idx++) {
                mfxRes = CreateConfig<mfxHDL>(par.ExtParam[idx], "ExtBuffer");
                if (MFX_ERR_NONE != mfxRes)
                    return MFX_ERR_UNSUPPORTED;
            }
        }

        // create session with highest priority implementation remaining after filters
        mfxRes = MFXCreateSession(m_loader, 0, &m_session);

        return mfxRes;
    }
#endif

private:
    MFXVideoSession(const MFXVideoSession &);
    void operator=(MFXVideoSession &);
};

class MFXVideoENCODE : public MFXVideoENCODEBase {
public:
    explicit MFXVideoENCODE(mfxSession session) {
        m_session = session;
    }
    virtual ~MFXVideoENCODE(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override {
        return MFXVideoENCODE_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) override {
        return MFXVideoENCODE_QueryIOSurf(m_session, par, request);
    }
    virtual mfxStatus Init(mfxVideoParam *par) override {
        return MFXVideoENCODE_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par) override {
        return MFXVideoENCODE_Reset(m_session, par);
    }
    virtual mfxStatus Close(void) override {
        return MFXVideoENCODE_Close(m_session);
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override {
        return MFXVideoENCODE_GetVideoParam(m_session, par);
    }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat) override {
        return MFXVideoENCODE_GetEncodeStat(m_session, stat);
    }

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl,
                                       mfxFrameSurface1 *surface,
                                       mfxBitstream *bs,
                                       mfxSyncPoint *syncp) override {
        return MFXVideoENCODE_EncodeFrameAsync(m_session, ctrl, surface, bs, syncp);
    }

    virtual mfxStatus GetSurface(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForEncode(m_session, output_surf);
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
private:
    MFXVideoENCODE(const MFXVideoENCODE& other);
    MFXVideoENCODE& operator=(const MFXVideoENCODE& other);
};

class MFXVideoDECODE : public MFXVideoDECODEBase {
public:
    explicit MFXVideoDECODE(mfxSession session) {
        m_session = session;
    }
    virtual ~MFXVideoDECODE(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override {
        return MFXVideoDECODE_Query(m_session, in, out);
    }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) override {
        return MFXVideoDECODE_DecodeHeader(m_session, bs, par);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) override {
        return MFXVideoDECODE_QueryIOSurf(m_session, par, request);
    }
    virtual mfxStatus Init(mfxVideoParam *par) override {
        return MFXVideoDECODE_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par) override {
        return MFXVideoDECODE_Reset(m_session, par);
    }
    virtual mfxStatus Close(void) override {
        return MFXVideoDECODE_Close(m_session);
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override {
        return MFXVideoDECODE_GetVideoParam(m_session, par);
    }

    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) override {
        return MFXVideoDECODE_GetDecodeStat(m_session, stat);
    }
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) override {
        return MFXVideoDECODE_GetPayload(m_session, ts, payload);
    }
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) override {
        return MFXVideoDECODE_SetSkipMode(m_session, mode);
    }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream *bs,
                                       mfxFrameSurface1 *surface_work,
                                       mfxFrameSurface1 **surface_out,
                                       mfxSyncPoint *syncp) override {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, syncp);
    }

    virtual mfxStatus GetSurface(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForDecode(m_session, output_surf);
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
private:
    MFXVideoDECODE(const MFXVideoDECODE& other);
    MFXVideoDECODE& operator=(const MFXVideoDECODE& other);
};

class MFXVideoVPP : public MFXVideoVPPBase {
public:
    explicit MFXVideoVPP(mfxSession session) {
        m_session = session;
    }
    virtual ~MFXVideoVPP(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override {
        return MFXVideoVPP_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest request[2]) override {
        return MFXVideoVPP_QueryIOSurf(m_session, par, request);
    }
    virtual mfxStatus Init(mfxVideoParam *par) override {
        return MFXVideoVPP_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par) override {
        return MFXVideoVPP_Reset(m_session, par);
    }
    virtual mfxStatus Close(void) override {
        return MFXVideoVPP_Close(m_session);
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override {
        return MFXVideoVPP_GetVideoParam(m_session, par);
    }
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat) override {
        return MFXVideoVPP_GetVPPStat(m_session, stat);
    }
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in,
                                       mfxFrameSurface1 *out,
                                       mfxExtVppAuxData *aux,
                                       mfxSyncPoint *syncp) override {
        return MFXVideoVPP_RunFrameVPPAsync(m_session, in, out, aux, syncp);
    }

    virtual mfxStatus GetSurfaceIn(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForVPP(m_session, output_surf);
    }
    virtual mfxStatus GetSurfaceOut(mfxFrameSurface1** output_surf) override {
        return MFXMemory_GetSurfaceForVPPOut(m_session, output_surf);
    }

    virtual mfxStatus ProcessFrameAsync(mfxFrameSurface1 *in, mfxFrameSurface1 **out) override {
        return MFXVideoVPP_ProcessFrameAsync(m_session, in, out);
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
private:
    MFXVideoVPP(const MFXVideoVPP& other);
    MFXVideoVPP& operator=(const MFXVideoVPP& other);
};

class MFXVideoDECODE_VPP
{
public:
    explicit MFXVideoDECODE_VPP(mfxSession session) {
        m_session = session;
    }
    virtual ~MFXVideoDECODE_VPP(void) {
        Close();
    }

    virtual mfxStatus Init(mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par) {
        return MFXVideoDECODE_VPP_Init(m_session, decode_par, vpp_par_array, num_channel_par);
    }
    virtual mfxStatus Reset(mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par) {
        return MFXVideoDECODE_VPP_Reset(m_session, decode_par, vpp_par_array, num_channel_par);
    }
    virtual mfxStatus GetChannelParam(mfxVideoChannelParam *par, mfxU32 channel_id) {
        return MFXVideoDECODE_VPP_GetChannelParam(m_session, par, channel_id);
    }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream *bs, mfxU32* skip_channels, mfxU32 num_skip_channels, mfxSurfaceArray **surf_array_out) {
        return MFXVideoDECODE_VPP_DecodeFrameAsync(m_session, bs, skip_channels, num_skip_channels, surf_array_out);
    }

    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) {
        return MFXVideoDECODE_VPP_DecodeHeader(m_session, bs, par);
    }
    virtual mfxStatus Close(void) {
        return MFXVideoDECODE_VPP_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) {
        return MFXVideoDECODE_VPP_GetVideoParam(m_session, par);
    }
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) {
        return MFXVideoDECODE_VPP_GetDecodeStat(m_session, stat);
    }
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) {
        return MFXVideoDECODE_VPP_GetPayload(m_session, ts, payload);
    }
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) {
        return MFXVideoDECODE_VPP_SetSkipMode(m_session, mode);
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
private:
    MFXVideoDECODE_VPP(const MFXVideoDECODE_VPP& other);
    MFXVideoDECODE_VPP& operator=(const MFXVideoDECODE_VPP& other);
};

#endif //__MFXVIDEOPLUSPLUS_H
