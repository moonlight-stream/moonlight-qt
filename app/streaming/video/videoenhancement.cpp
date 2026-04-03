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
 * \param int adapterIndex
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
 * \param int vendorId
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
 * \brief Check if the vendor is Qualcomm
 *
 * \return bool Returns true is the vendor is Qualcomm
 */
bool VideoEnhancement::isVendorQualcomm(){
    return m_VendorId == VENDOR_ID_QUALCOMM;
}

/**
 * \brief Set the Video Super-Resolution availability
 *
 * Keep track if the adapter has Video Super-Resolution available
 *
 * \param bool available, at False the feature is uncheckable in the setting
 * \return void
 */
void VideoEnhancement::setAvailable(bool available){
    m_Available = available;
}

/**
 * \brief Check the Video Super-Resolution availability
 *
 * Check if the GPU adapter has Video Super-Resolution available
 *
 * \return bool Returns true if the VSR feature is available
 */
bool VideoEnhancement::isAvailable(){
    return m_Available;
}

/**
 * \brief Set the Video Super-Resolution capability
 *
 * Keep track if the adapter is capable of the Driver Video Super-Resolution
 *
 * \param bool capable
 * \return void
 */
void VideoEnhancement::setVSRcapable(bool capable){
    m_VSRcapable = capable;
}

/**
 * \brief Check the Video Super-Resolution capability
 *
 * Check if the GPU used is capable of providing Driver VSR feature
 *
 * \return bool Returns true if the Driver VSR feature is available
 */
bool VideoEnhancement::isVSRcapable(){
    return m_VSRcapable;
}

/**
 * \brief Set the HDR capability
 *
 * Keep track if the adapter is capable of SDR to HDR by the Driver
 *
 * \param bool capable
 * \return void
 */
void VideoEnhancement::setHDRcapable(bool capable){
    m_HDRcapable = capable;
}

/**
 * \brief Check the HDR capability
 *
 * Check if the GPU used is capable of providing the Driver SDR to HDR feature
 *
 * \return bool Returns true if the HDR feature is available
 */
bool VideoEnhancement::isHDRcapable(){
    return m_HDRcapable;
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
    m_Enabled = m_Available && activate;
    return m_Enabled;
}

/**
 * \brief Set the upscaling Ratio
 *
 * Set the value of the ratio which must be TextureOutputHeight/TextureInputHeight.
 *
 * \param float ratio
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
 * \param std::string algo
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
 * \param bool isIntegratedGPU
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
 * \brief Set AV Hardware Device Type
 *
 * Keep the information about the Device Type used
 *
 * \param int deviceType
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
