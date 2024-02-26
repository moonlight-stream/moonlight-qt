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
    bool m_UIvisible = false;

    // Vendors' name (PCI Special Interest Group)
    const int VENDOR_ID_AMD = 0x1002;
    const int VENDOR_ID_INTEL = 0x8086;
    const int VENDOR_ID_NVIDIA = 0x10DE;

    // Minimum driver version accepted for VSR feature
    const double MIN_VSR_DRIVER_VERSION_AMD = 21910.5; // AMD Driver Version 23.19.10 (Jan 23rd, 2024)
    const double MIN_VSR_DRIVER_VERSION_INTEL = 100.8681; // Intel Driver Version 27.20.100.8681 (Sept 15, 2020)
    const double MIN_VSR_DRIVER_VERSION_NVIDIA = 15.4584; // NVIDIA Driver Version 545.84 (Oct 13, 2023)

    // Minimum driver version accepted for HDR feature
    const double MIN_HDR_DRIVER_VERSION_AMD = 0; // To be determined, this feature has not yet been announced by AMD
    const double MIN_HDR_DRIVER_VERSION_INTEL = 0; // To be determined, this feature has not yet been announced by Intel
    const double MIN_HDR_DRIVER_VERSION_NVIDIA = 15.5123; // https://www.nvidia.com/download/driverResults.aspx/218114/en-us/

    // GPU information
    int m_VendorId = 0;
    std::wstring m_GPUname = L"Unknown";
    double m_DriverVersion = 0;

    // Disable the constructor from outside to avoid a new instance
    VideoEnhancement();

    // Private copy constructor and assignment operator to prevent duplication
    VideoEnhancement(const VideoEnhancement&);
    VideoEnhancement& operator=(const VideoEnhancement&);

    bool setGPUinformation();
    int getVideoDriverInfo();

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
