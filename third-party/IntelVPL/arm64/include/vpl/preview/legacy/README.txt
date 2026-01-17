The file preview/legacy/mfxvideo++.h is a preview implementation of "class
MFXVideoSession" which takes advantage of API 2.0 functions of the
Intel® Video Processing Library (Intel® VPL) for implementation selection and
session creation.

Known limitations:

- The parameter mfxInitParam.ExternalThreads is not supported.

- The API version returned by MFXVideoSession::QueryVersion() may be different
  on platforms for which libmfx-gen is the default runtime implementation.
  
- On Windows, accelerators selected using MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE2,
  MFX_IMPL_HARDWARE3, or MFX_IMPL_HARDWARE4 are always enumerated according to
  IDXGIFactory::EnumAdapters (i.e. D3D11) indexes, regardless of the
  acceleration mode selected.  On a multi-monitor or multi-adapter system, D3D9
  and D3D11 adapter indexing may not match. Applications needing to create a
  session on a specific D3D9 adapter should instead use the Dispatcher
  Configuration Property mfxExtendedDeviceId.DeviceLUID to select the desired
  adapter.

- Identical behavior between the production implementation of mfxvideo++.h and
  the preview is not guaranteed. Applications may however define
  MFXVIDEO_CPP_USE_DEPRECATED when compiling the preview application to build
  with the previous implementation.
