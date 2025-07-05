#include "videoenhancement.h"

/**
 * \brief Constructor (Singleton)
 *
 * VideoEnhancement does not set its properties automatically at instance initiation,
 * it depends on D3D11va. Therefore, it needs to be populated at the initialization of
 * the rendered D3D11VARenderer::initialize().
 *
 * \return void
 */
VideoEnhancement::VideoEnhancement(){}

/**
 * \brief Get the singleton instance
 *
 * Render the instance of the singleton
 *
 * \return VideoEnhancement instance
 */
VideoEnhancement &VideoEnhancement::getInstance(){
    static VideoEnhancement instance;
    return instance;
}

/**
 * \brief Set the Adapter Index
 *
 * \return void
 */
void VideoEnhancement::setAdapterIndex(int adapterIndex){
    m_AdapterIndex = adapterIndex;
}

/**
 * \brief Get the Adapter Index
 *
 * \return int Returns the Index of the most capable adapter for Video enhancement
 */
int VideoEnhancement::getAdapterIndex(){
    return m_AdapterIndex;
}

/**
 * \brief Set Vendor ID
 *
 * \return void
 */
void VideoEnhancement::setVendorID(int vendorId){
    m_VendorId = vendorId;
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
 * \brief Check if the vendor is AMD
 *
 * \param int vendorId Vendor ID
 * \return bool Returns true is the vendor is AMD
 */
bool VideoEnhancement::isVendorAMD(int vendorId){
    return vendorId == VENDOR_ID_AMD;
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
 * \brief Check if the vendor is Intel
 *
 * \param int vendorId Vendor ID
 * \return bool Returns true is the vendor is Intel
 */
bool VideoEnhancement::isVendorIntel(int vendorId){
    return vendorId == VENDOR_ID_INTEL;
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
 * \brief Check if the vendor is NVIDIA
 *
 * \param int vendorId Vendor ID
 * \return bool Returns true is the vendor is NVIDIA
 */
bool VideoEnhancement::isVendorNVIDIA(int vendorId){
    return vendorId == VENDOR_ID_NVIDIA;
}

/**
 * \brief Set the Video Super-Resolution capability
 *
 * Keep track if the adapter is capable of Video Super-Resolution
 *
 * \return void
 */
void VideoEnhancement::setVSRcapable(bool capable){
    m_VSRcapable = capable;
}

/**
 * \brief Check the Video Super-Resolution capability
 *
 * Check if the GPU used is capable of providing VSR feature
 *
 * \return bool Returns true if the VSR feature is available
 */
bool VideoEnhancement::isVSRcapable(){
    return m_VSRcapable;
}

/**
 * \brief Set the HDR capability
 *
 * Keep track if the adapter is capable of SDR to HDR
 *
 * \return void
 */
void VideoEnhancement::setHDRcapable(bool capable){
    m_HDRcapable = capable;
}

/**
 * \brief Check the HDR capability
 *
 * Check if the GPU used is capable of providing SDR to HDR feature
 *
 * \return bool Returns true if the HDR feature is available
 */
bool VideoEnhancement::isHDRcapable(){
    return m_HDRcapable;
}

/**
 * \brief Check the AI-Enhancement capability
 *
 * Check if the GPU used is capable of enhancing the video
 *
 * \return bool Returns true if the such capability is available
 */
bool VideoEnhancement::isEnhancementCapable(){
    return m_VSRcapable || m_HDRcapable;
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
 * \return void
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
    // Only Intel is experimental, NVIDIA and AMD are official
    // [ToDo] If Intel officially release the feature, we can return false or just delete
    // this method and the QML logic associated.
    return isVendorIntel();
}
