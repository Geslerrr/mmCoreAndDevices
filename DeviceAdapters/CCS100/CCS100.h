#pragma once

#include "MMDevice.h"
#include "DeviceBase.h"
#include <string>
#include <vector>

class CCS100 : public CCameraBase<CCS100>
{
public:
    CCS100();
    ~CCS100();

    // MMDevice API
    int Initialize() override;
    int Shutdown() override;
    void GetName(char* name) const override;

    // Camera API
    int SnapImage() override;
    const unsigned char* GetImageBuffer() override;
    unsigned GetImageWidth() const override;
    unsigned GetImageHeight() const override;
    unsigned GetImageBytesPerPixel() const override;
    long GetImageBufferSize() const override;

    bool Busy() override;

    // Exposure
    void SetExposure(double exposure) override;
    double GetExposure() const override;
    int IsExposureSequenceable(bool& isSequenceable) const override;

    // ROI
    int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize) override;
    int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize) override;
    int ClearROI() override;

    // Binning
    int SetBinning(int binSize) override;
    int GetBinning() const override;

    // Bit depth
    unsigned GetBitDepth() const override;

    // Sequence acquisition
    int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow) override;
    int StartSequenceAcquisition(double interval_ms) override;
    int StopSequenceAcquisition() override;
    bool IsCapturing() override;

    unsigned GetNumberOfComponents() const override;

    int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnWavelengthChunkIndex(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnWavelengthChunk(MM::PropertyBase* pProp, MM::ActionType eAct);


private:

    ViSession session_;
    bool initialized_;

    static const unsigned RAW_PIXELS = 3694; //unused
    static const unsigned ACTIVE_PIXELS = 3648;

    static const unsigned WIDTH = 3648;
    static const unsigned HEIGHT = 1;

    ViReal64 raw_[ACTIVE_PIXELS];
    ViReal64 wavelengthData_[ACTIVE_PIXELS];
    ViReal64 temp_[ACTIVE_PIXELS];
    uint16_t image_[ACTIVE_PIXELS];

    static const unsigned WAVELENGTHS_PER_CHUNK = 40;
    long wavelengthChunkIndex_ = 0;
    std::vector<std::string> wavelengthChunks_;

    double exposureMs_ = 1.0;

    bool stop_;

    class AcqThread : public MMDeviceThreadBase
    {
    public:
        AcqThread(CCS100& device) :
            device_(device),
            stop_(false)
        {
        }

        int svc() override;
        void Stop() { stop_ = true; }

    private:
        CCS100& device_;
        bool stop_; //probably don't need the local stop_, but it's working and a pain to change.
    };


    AcqThread* acqThread_;
    bool activeThread_;
};
