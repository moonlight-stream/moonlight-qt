#ifndef VIDEOENHANCEMENT_H
#define VIDEOENHANCEMENT_H

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>

class VideoEnhancement : public QObject
{
    Q_OBJECT

private:

    static VideoEnhancement* instance;

    bool m_Initialized = false;
    bool m_Enabled = false;
    bool m_UIvisible = true; // [Bruno] It should be false, and turn true by dxva2.cpp

    // Vendors' name (PCI Special Interest Group)
    const int VENDOR_ID_AMD = 4098;
    const int VENDOR_ID_INTEL = 32902;
    const int VENDOR_ID_NVIDIA = 4318;

    // Minimum driver version accepted for VSR feature
    const int MIN_VSR_DRIVER_VERSION_AMD = 24; // It is implemented from the driver 24.1.1
    const int MIN_VSR_DRIVER_VERSION_INTEL = 28; // It will ensure to cover 27.20 version
    const int MIN_VSR_DRIVER_VERSION_NVIDIA = 54584; // NVIDIA driver name are always in the format XXX.XX (https://www.nvidia.com/en-gb/drivers/drivers-faq/)

    // Minimum driver version accepted for HDR feature
    const int MIN_HDR_DRIVER_VERSION_AMD = 0; // To be determined, this feature has not yet been announced by AMD
    const int MIN_HDR_DRIVER_VERSION_INTEL = 0; // To be determined, this feature has not yet been announced by Intel
    const int MIN_HDR_DRIVER_VERSION_NVIDIA = 55123; // https://www.nvidia.com/download/driverResults.aspx/218114/en-us/

    // GPU information
    int m_VendorId = 0;
    std::wstring m_GPUname = L"Unknown";
    int m_DriverVersion = 0;

    // Disable the constructor from outside to avoid a new instance
    VideoEnhancement();

    // Private copy constructor and assignment operator to prevent duplication
    VideoEnhancement(const VideoEnhancement&);
    VideoEnhancement& operator=(const VideoEnhancement&);

    bool setGPUinformation();
    int GetVideoDriverInfo();

public:
    static VideoEnhancement& getInstance();
    bool isVendorAMD();
    bool isVendorIntel();
    bool isVendorNVIDIA();
    bool isEnhancementCapable();
    bool isVSRcapable();
    bool isHDRcapable();
    bool isVideoEnhancementEnabled();
    bool enableVideoEnhancement(bool activate = true);
    void enableUIvisible(bool visible = true);

    Q_INVOKABLE bool isUIvisible();
    Q_INVOKABLE bool isExperimental();

};

#endif // VIDEOENHANCEMENT_H
