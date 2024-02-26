#include "videoenhancement.h"
#include <QtDebug>
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>

#include <wrl/client.h>

#pragma comment(lib, "Advapi32.lib")

/**
 * \brief Constructor (Singleton)
 *
 * Check the capacity to handle the AI-Enhancement features such as Video Super-Resolution or SDR to HDR, according to multiple parameters such as OS or Video driver.
 *
 * \return void
 */
VideoEnhancement::VideoEnhancement()
{
    if(!m_Initialized){
        setGPUinformation();
        // Avoid to set variables every call of the instance
        m_Initialized = true;
    }
}

/**
 * \brief Get the singleton instance
 *
 * Render the instance of the singleton
 *
 * \return VideoEnhancement instance
 */
VideoEnhancement &VideoEnhancement::getInstance()
{
    static VideoEnhancement instance;
    return instance;
}

/**
 * \brief Retreive GPU information
 *
 * Retreive all GPU information: Vendor ID, Driver version, GPU name
 *
 * \return bool Returns true if it successfully retreived the GPU information
 */
bool VideoEnhancement::setGPUinformation()
{
    bool success = false;

    // Create a Direct3D 11 device
    ID3D11Device* pD3DDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &pD3DDevice,
        nullptr,
        &pContext
        );

    IDXGIAdapter* pAdapter = nullptr;
    IDXGIDevice* pDXGIDevice = nullptr;
    // Get the DXGI device from the D3D11 device.
    // It identifies which GPU is being used by the application in case of multiple one (like a iGPU with a dedicated GPU).
    if (SUCCEEDED(hr) && SUCCEEDED(pD3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice))) {
        // Get the DXGI adapter from the DXGI device
        if (SUCCEEDED(pDXGIDevice->GetAdapter(&pAdapter))) {
            DXGI_ADAPTER_DESC adapterIdentifier;
            if (SUCCEEDED(pAdapter->GetDesc(&adapterIdentifier))) {
                // Convert wchar[128] to string
                std::wstring description(adapterIdentifier.Description);

                // Set GPU information
                m_VendorId = adapterIdentifier.VendorId;
                m_GPUname = description;

                LARGE_INTEGER umd_version;
                pAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd_version);
                // Integer part
                m_DriverVersion = HIWORD(umd_version.LowPart);
                // Decimal part
                double digits = static_cast<double>(floor(log10(LOWORD(umd_version.LowPart))) + 1);
                if(digits > 0){
                    m_DriverVersion += LOWORD(umd_version.LowPart) / pow(10, digits);
                }

                qInfo() << "Active GPU: " << m_GPUname;
                qInfo() << "Video Driver: " << m_DriverVersion;

            }
        }
    }

    // Release resources
    if (pD3DDevice) pD3DDevice->Release();
    if (pDXGIDevice) pDXGIDevice->Release();
    if (pAdapter) pAdapter->Release();

    return success;
}

/**
 * \brief Check if the vendor is AMD
 *
 * \return bool Returns true is the vendor is AMD
 */
bool VideoEnhancement::isVendorAMD(){
    return m_VendorId == VENDOR_ID_AMD;
}

/**
 * \brief Check if the vendor is Intel
 *
 * \return bool Returns true is the vendor is Intel
 */
bool VideoEnhancement::isVendorIntel(){
    return m_VendorId == VENDOR_ID_INTEL;
}

/**
 * \brief Check if the vendor is NVIDIA
 *
 * \return bool Returns true is the vendor is NVIDIA
 */
bool VideoEnhancement::isVendorNVIDIA(){
    return m_VendorId == VENDOR_ID_NVIDIA;
}

/**
 * \brief Check the Video Super-Resolution capability
 *
 * Check if the GPU used is capable of providing VSR feature according to its serie or driver version
 *
 * \return bool Returns true if the VSR feature is available
 */
bool VideoEnhancement::isVSRcapable(){
    if(isVendorAMD()){
        // [TODO] To be done once AMD provides the VSR solution
        // Driver > 24 && RX 7000+
    } else if(isVendorIntel()){
        // All CPU with iGPU (Gen 10th), or dedicated GPU, are capable
        if(m_DriverVersion >= MIN_VSR_DRIVER_VERSION_INTEL){
            return true;
        }
    } else if(isVendorNVIDIA()){
        // RTX VSR v1.5 supports NVIDIA RTX Series 20 starting from the Windows drive 545.84 (Oct 17, 2023).
        if(
            m_GPUname.find(L"RTX") != std::wstring::npos
            && m_DriverVersion >= MIN_VSR_DRIVER_VERSION_NVIDIA
            ){
            return true;
        }
    }
    return false;
}

/**
 * \brief Check the HDR capability
 *
 * Check if the GPU used is capable of providing SDR to HDR feature according to its serie or driver version
 *
 * \return bool Returns true if the HDR feature is available
 */
bool VideoEnhancement::isHDRcapable(){
    if(isVendorAMD()){
        // Not yet announced by AMD
    } else if(isVendorIntel()){
        // Not yet announced by Intel
    } else if(isVendorNVIDIA()){
        // RTX VSR v1.5 supports NVIDIA RTX Series 20 starting from the Windows drive 545.84 (Oct 17, 2023).
        if(
            m_GPUname.find(L"RTX") != std::wstring::npos
            && m_DriverVersion >= MIN_HDR_DRIVER_VERSION_NVIDIA
            ){
            return true;
        }
    }
    return false;
}

/**
 * \brief Check the AI-Enhancement capability
 *
 * Check if the GPU used is capable of enhancing the video
 *
 * \return bool Returns true if the such capability is available
 */
bool VideoEnhancement::isEnhancementCapable(){
    return isVSRcapable() || isHDRcapable();
}

/**
 * \brief Check if Video Enhancement feature is enabled
 *
 * \return bool Returns true if the Video Enhancement feature is enabled
 */
bool VideoEnhancement::isVideoEnhancementEnabled(){
    return m_Enabled;
}

/**
 * \brief Enable Video Enhancement feature
 *
 * \param bool activate Default is true, at true it enables the use of Video Enhancement feature
 * \return bool Returns true if the Video Enhancement feature is available
 */
bool VideoEnhancement::enableVideoEnhancement(bool activate){
    m_Enabled = isEnhancementCapable() && activate;
    return m_Enabled;
}

/**
 * \brief Enable Video Enhancement accessibility from the settings interface
 *
 * \param bool visible Default is true, at true it displays Video Enhancement feature
 * \return bool Returns true if the Video Enhancement feature is available
 */
void VideoEnhancement::enableUIvisible(bool visible){
    m_UIvisible = visible;
}

/**
 * \brief Check if Video Enhancement feature is accessible from the settings interface
 *
 * \return bool Returns true if the Video Enhancement feature is accessible
 */
bool VideoEnhancement::isUIvisible(){
    return m_UIvisible;
}

/**
 * \brief Check if Video Enhancement feature is experimental from the vendor
 *
 * \return bool Returns true if the Video Enhancement feature is experimental
 */
bool VideoEnhancement::isExperimental(){
    // [Jan 31st 2024] AMD's is not yet available, Intel's is experimental, NVIDIA's is official
    return isVendorIntel();
}
