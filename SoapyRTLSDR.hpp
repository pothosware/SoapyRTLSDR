/*
 * TODO license, copyright, etc
 * 
 */

#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <rtl-sdr.h>

typedef enum rtlsdrRXFormat { RTL_RX_FORMAT_FLOAT32, RTL_RX_FORMAT_FLOAT16, RTL_RX_FORMAT_INT16 } rtlsdrRXFormat;

#define DEFAULT_BUFFER_LENGTH 16384
#define DEFAULT_NUM_BUFFERS 16

class SoapyRTLSDR : public SoapySDR::Device
{
public:
    SoapyRTLSDR(const SoapySDR::Kwargs &args);

    ~SoapyRTLSDR(void);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    SoapySDR::Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels = std::vector<size_t>(),
        const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0,
        const size_t numElems = 0);

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0);

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs = 100000);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

//    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);
//
//    bool getDCOffsetMode(const int direction, const size_t channel) const;
//
//    bool hasDCOffset(const int direction, const size_t channel) const;
//
//    void setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset);
//
//    std::complex<double> getDCOffset(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const double value);

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;

private:

    //device handle
    int deviceId;
    rtlsdr_dev_t *dev;

    //cached settings
    rtlsdrRXFormat rxFormat;
    uint32_t sampleRate, centerFrequency;
    int ppm, directSamplingMode, bufferSize, numBuffers, bufferLength;
    bool iqSwap, agcMode, offsetMode;

    //next state
    uint32_t newSampleRate, newCenterFrequency;
    int newPpm, newDirectSamplingMode;
    bool newAgcMode, newOffsetMode;

    // state change
    bool sampleRateChanged, centerFrequencyChanged, ppmChanged;
    bool iqSwapChanged, directSamplingModeChanged, agcModeChanged;
    bool offsetModeChanged;

    // buffers
    std::vector<signed char> iq_input;
    std::vector<signed char> iq_buffer;

    std::vector<std::complex<float> > _lut_32f;
    std::vector<std::complex<float> > _lut_swap_32f;
    std::vector<std::complex<short int> > _lut_16i;
    std::vector<std::complex<short int> > _lut_swap_16i;

public:
    static int rtl_count;
    static std::vector< SoapySDR::Kwargs > rtl_devices;
};
