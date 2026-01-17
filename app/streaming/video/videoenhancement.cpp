#include "videoenhancement.h"
#include <string>

/**
 * \brief Constructor (Singleton)
 *
 * VideoEnhancement does not set its properties automatically at instance initiation.
 * Therefore, it needs to be populated at the initialization of
 * the renderer who makes use of it.
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
 * \brief Force the Video Super-Resolution capability
 *
 * If VideoProcessor Extesion are not available, we still allow Video Super-Resolution
 * thanks to the Shader fallback.
 *
 * \return void
 */
void VideoEnhancement::setForceCapable(bool capable){
    m_ForceCapable = capable;
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
 * \brief Check the Video-Enhancement capability
 *
 * Check if the GPU used is capable of enhancing the video
 *
 * \return bool Returns true if the such capability is available
 */
bool VideoEnhancement::isEnhancementCapable(){
    return m_ForceCapable || m_VSRcapable || m_HDRcapable;
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
    // No vendor is in experimental mode
    return false;
}

/**
 * \brief Set the upscaling Ratio
 *
 * Set the value of the ratio which must be TextureOutputHeight/TextureInputHeight.
 *
 * \return void
 */
void VideoEnhancement::setRatio(float ratio){
    m_Ratio = ratio;
}

/**
 * \brief Get the upscaling Ratio
 *
 * Return the value of the upscaling ratio Output/Input.
 *
 * \return float Returns the upscaling Ratio
 */
float VideoEnhancement::getRatio(){
    return m_Ratio;
}

/**
 * \brief Set the upscaling Algorythm
 *
 * Set the value of the algorythm used to do the upscaling.
 *
 * \return void
 */
void VideoEnhancement::setAlgo(std::string algo){
    m_Algo = algo;
}

/**
 * \brief Get the upscaling Algorythm
 *
 * Return the value of algorythm used to do the upscaling.
 *
 * \return float Returns the upscaling Algorythm
 */
std::string VideoEnhancement::getAlgo(){
    return m_Algo;
}

/**
 * \brief Set Integrated GPU
 *
 * Set the information that the GPU is integrated to the CPU (iGPU), or a discrete GPU
 *
 * \return void
 */
void VideoEnhancement::setIntegratedGPU(bool isIntegratedGPU){
    m_IsIntegratedGPU = isIntegratedGPU;
}

/**
 * \brief Get Integrated GPU
 *
 * \return bool Returns true for iGPU, false for dGPU
 */
bool VideoEnhancement::isIntegratedGPU(){
    return m_IsIntegratedGPU;
}

/**
 * \brief Set Number of Frames
 *
 * Set the number of Frames sent
 *
 * \return void
 */
void VideoEnhancement::resetNumFrame(){
    m_NumFrame = 0;
}

/**
 * \brief Number of Frame
 *
 * \return int Increments the number of frames sent
 */
long VideoEnhancement::incrementNumFrame(){
    return m_NumFrame++;
}

/**
 * \brief Number of Frame
 *
 * \return int Returns the number of frames sent
 */
long VideoEnhancement::getNumFrame(){
    return m_NumFrame;
}

/**
 * \brief Set AV Hardware Device Type
 *
 * Keep the information about the Device Type used
 *
 * \param int equivant to AVHWDeviceType
 * \return void
 */
void VideoEnhancement::setDeviceType(int deviceType){
    m_DeviceType = deviceType;
}

/**
 * \brief Get AV Hardware Device Type
 *
 * \return int Returns the Device type used, equivalent to AVHWDeviceType
 */
int VideoEnhancement::getDeviceType(){
    return m_DeviceType;
}
