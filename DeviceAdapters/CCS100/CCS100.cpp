#include <visa.h>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

extern "C" {
#include "TLCCS.h"
}

#include "CCS100.h"

const char* g_DeviceName = "CCS100";

// Required by Micro-Manager
MODULE_API void InitializeModuleData()
{
    RegisterDevice(g_DeviceName, MM::CameraDevice, "Thorlabs CCS100 Spectrometer");
}

MODULE_API MM::Device* CreateDevice(const char* name)
{
    if (strcmp(name, g_DeviceName) == 0)
        return new CCS100();
    return nullptr;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
}

// ---- CCS100 class ----

CCS100::CCS100()
    : initialized_(false),
      wavelengthChunkIndex_(0)
{
    CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false);
    AddAllowedValue(MM::g_Keyword_Binning, "1");
    CreateProperty("WavelengthChunkIndex", "0", MM::Integer, false,
        new CPropertyAction(this, &CCS100::OnWavelengthChunkIndex));
    CreateProperty("WavelengthChunk", "", MM::String, true,
        new CPropertyAction(this, &CCS100::OnWavelengthChunk));
}

CCS100::~CCS100()
{
}

void CCS100::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceName);
}

int CCS100::Initialize()
{
    ViStatus status = tlccs_init(const_cast<char*>("USB0::0x1313::0x8081::M01242067::RAW"), VI_ON, VI_ON, &session_);
    if (status != VI_SUCCESS) {
        LogMessage("tlccs_init failed: " + std::to_string(status));
        return DEVICE_ERR;
    }

    ViReal64 minWavelength;
    ViReal64 maxWavelength;
    status = tlccs_getWavelengthData(session_, 0, wavelengthData_,
        &minWavelength, &maxWavelength);
    if (status != VI_SUCCESS) {
        LogMessage("tlccs_getWavelengthData failed: " + std::to_string(status));
        tlccs_close(session_);
        return DEVICE_ERR;
    }

    wavelengthChunks_.clear();
    const unsigned chunkCount = (ACTIVE_PIXELS + WAVELENGTHS_PER_CHUNK - 1) /
        WAVELENGTHS_PER_CHUNK;
    wavelengthChunks_.reserve(chunkCount);
    for (unsigned chunk = 0; chunk < chunkCount; ++chunk) {
        const unsigned first = chunk * WAVELENGTHS_PER_CHUNK;
        const unsigned end = (first + WAVELENGTHS_PER_CHUNK < ACTIVE_PIXELS)
            ? first + WAVELENGTHS_PER_CHUNK : ACTIVE_PIXELS;
        std::ostringstream values;
        values << std::setprecision(std::numeric_limits<ViReal64>::max_digits10);
        for (unsigned i = first; i < end; ++i) {
            if (i != first)
                values << ',';
            values << wavelengthData_[i];
        }
        wavelengthChunks_.push_back(values.str());
    }

    initialized_ = true;
    SetExposure(1.0);
    LogMessage("tlccs_init and wavelength calibration succeeded");
    return DEVICE_OK;
}

int CCS100::OnWavelengthChunkIndex(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet) {
        pProp->Set(wavelengthChunkIndex_);
    }
    else if (eAct == MM::AfterSet) {
        long index;
        pProp->Get(index);
        if (index < 0 || static_cast<size_t>(index) >= wavelengthChunks_.size())
            return DEVICE_INVALID_PROPERTY_VALUE;
        wavelengthChunkIndex_ = index;
    }
    return DEVICE_OK;
}

int CCS100::OnWavelengthChunk(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct != MM::BeforeGet)
        return DEVICE_OK;
    if (!initialized_)
        return DEVICE_NOT_CONNECTED;
    if (wavelengthChunkIndex_ < 0 ||
        static_cast<size_t>(wavelengthChunkIndex_) >= wavelengthChunks_.size())
        return DEVICE_INVALID_PROPERTY_VALUE;
    pProp->Set(wavelengthChunks_[wavelengthChunkIndex_].c_str());
    return DEVICE_OK;
}

int CCS100::Shutdown() //done
{
    tlccs_close(session_);

    return DEVICE_OK;
}

int CCS100::SnapImage()
{
    //LogMessage("function called");
    if (!initialized_) {
        LogMessage("not initialized");
        return DEVICE_NOT_CONNECTED;
    }

    ViStatus status;
    int counter = 0;


    //LogMessage("right before scan");
    status = tlccs_startScan(session_); //start scan

    //LogMessage("scan started: " + std::to_string(status));

    ViInt32 scanState;
    do {
        tlccs_getDeviceStatus(session_, &scanState);
        counter = counter + 1;
        //LogMessage("polling: " + std::to_string(counter));

        if (status != VI_SUCCESS)
            return DEVICE_ERR;

        CDeviceUtils::SleepMs(1);
    } while (scanState == 0x0004 or scanState == 0x0008);  //this might be a source of error.....

    tlccs_getScanData(session_, raw_);
    //LogMessage("got data");


    for (unsigned i = 0; i < ACTIVE_PIXELS; ++i) //cast to image (16-bit int instead of 64 bit)
    {
        if (raw_[i] < 0.0) {  //clamping values below 0
            temp_[i] = 0.0;
        }
        else {
            temp_[i] = raw_[i];
        }
        temp_[i] = temp_[i] * 65535;

        image_[i] = static_cast<uint16_t>(temp_[i]);  //placeholder, remove later, or use for recasting

        //LogMessage("data" + std::to_string(i) + ":" + std::to_string(temp_[i]));
    }
    //LogMessage("recast complete");

    if (status != VI_SUCCESS)
        return DEVICE_ERR;

    return DEVICE_OK;
}

const unsigned char* CCS100::GetImageBuffer() //done?
{
    //LogMessage("'get image buffer'");
    return reinterpret_cast<const unsigned char*>(image_);
}

unsigned CCS100::GetImageWidth() const //done
{
    return WIDTH;
}

unsigned CCS100::GetImageHeight() const //done
{
    return HEIGHT;
}

unsigned CCS100::GetImageBytesPerPixel() const //done ****assuming 16-bit
{
    return 2; //4
}

long CCS100::GetImageBufferSize() const //done
{
    return WIDTH * HEIGHT * GetImageBytesPerPixel();
}

//busy
bool CCS100::Busy() //maybe change later
{
    return false;
}

//exposure
void CCS100::SetExposure(double exposure) //done
{
    exposureMs_ = exposure;
    tlccs_setIntegrationTime(session_, exposureMs_ / 1000); // divide by 1000 because input is millisec, tlccs is seconds.
}

double CCS100::GetExposure() const //done
{
    return exposureMs_;
}

int CCS100::IsExposureSequenceable(bool& isSequenceable) const //no need
{
    return 0;
}

//ROI
int CCS100::SetROI(unsigned, unsigned, unsigned, unsigned) //no need
{
    return DEVICE_ERR;
}

int CCS100::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize) //no need
{
    x = 0;
    y = 0;
    xSize = WIDTH;
    ySize = HEIGHT;
    return DEVICE_OK;
}

int CCS100::ClearROI() //no need
{
    return DEVICE_OK;
}

//binning
int CCS100::SetBinning(int) //no need
{
    return DEVICE_OK;
}

int CCS100::GetBinning() const //no need
{
    return 1;
}

//bit depth
unsigned CCS100::GetBitDepth() const //done ****assuming 16-bit
{
    return 16; //32
}

//sequence acquisition
int CCS100::StartSequenceAcquisition(long, double, bool) //may need later
{
    return DEVICE_ERR;
}

int CCS100::StartSequenceAcquisition(double) //may need later
{
    LogMessage("Start acquisition");
    stop_ = false;

    LogMessage("activethread?:" + std::to_string(activeThread_));
    if (acqThread_ && activeThread_)
        return DEVICE_ERR;

    acqThread_ = new AcqThread(*this);

    acqThread_->activate();
    activeThread_ = true;
    LogMessage("Activated thread!");

    return DEVICE_OK;



}

int CCS100::AcqThread::svc()
{
    tlccs_startScanCont(device_.session_);

    while (!device_.stop_)
    {
        tlccs_getScanData(device_.session_, device_.raw_);
        for (unsigned i = 0; i < ACTIVE_PIXELS; ++i) //cast to image (16-bit int instead of 64 bit)
        {
            if (device_.raw_[i] < 0.0) {  //clamping values below 0
                device_.temp_[i] = 0.0;
            }
            else {
                device_.temp_[i] = device_.raw_[i];
            }
            device_.temp_[i] = device_.temp_[i] * 65535;

            device_.image_[i] = static_cast<uint16_t>(device_.temp_[i]);  //placeholder, remove later, or use for recasting

        }

        int ret = AcqThread::device_.GetCoreCallback()->InsertImage(
            &device_,
            reinterpret_cast<const unsigned char*>(device_.image_),
            WIDTH,   // width
            HEIGHT,               // height
            2                // bytes per pixel
        );

        if (ret != DEVICE_OK)
            break;

        CDeviceUtils::SleepMs(1);
    }
    return DEVICE_OK;
}

int CCS100::StopSequenceAcquisition() //may need later
{
    LogMessage("Stop acquisition");
    stop_ = true;

    tlccs_reset(session_); //from docs - any function except get data/get status will stop the scan - random get function to stop the scan.

    if (acqThread_)
    {
        acqThread_->Stop();
        acqThread_->wait();   // <-- correct call
        delete acqThread_;
        acqThread_ = nullptr;
    }
    LogMessage("Stopped thread!");
    activeThread_ = false;
    return DEVICE_OK;
}

bool CCS100::IsCapturing() //may need later
{
    return activeThread_;
}

unsigned CCS100::GetNumberOfComponents() const
{
    return 1;
}
