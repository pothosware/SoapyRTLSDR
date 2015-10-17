/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <rtl-sdr.h>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>

typedef enum rtlsdrRXFormat
{
    RTL_RX_FORMAT_FLOAT32, RTL_RX_FORMAT_INT16, RTL_RX_FORMAT_INT8
} rtlsdrRXFormat;

#define DEFAULT_BUFFER_LENGTH 16384
#define DEFAULT_NUM_BUFFERS 15
#define BYTES_PER_SAMPLE 2

class SoapyRTLSDR: public SoapySDR::Device
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

    SoapySDR::Stream *setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels =
            std::vector<size_t>(), const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(
            SoapySDR::Stream *stream,
            const int flags = 0,
            const long long timeNs = 0,
            const size_t numElems = 0);

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0);

    int readStream(
            SoapySDR::Stream *stream,
            void * const *buffs,
            const size_t numElems,
            int &flags,
            long long &timeNs,
            const long timeoutUs = 100000);

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

    int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

    int acquireReadBuffer(
        SoapySDR::Stream *stream,
        size_t &handle,
        const void **buffs,
        int &flags,
        long long &timeNs,
        const long timeoutUs = 100000);

    void releaseReadBuffer(
        SoapySDR::Stream *stream,
        const size_t handle);

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

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    bool hasGainMode(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const double value);

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(
            const int direction,
            const size_t channel,
            const std::string &name,
            const double frequency,
            const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

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

    /*******************************************************************
     * Utility
     ******************************************************************/

    static std::string rtlTunerToString(rtlsdr_tuner tunerType);
    static rtlsdr_tuner rtlStringToTuner(std::string tunerType);

private:

    //device handle
    int deviceId;
    rtlsdr_dev_t *dev;

    //cached settings
    rtlsdrRXFormat rxFormat;
    rtlsdr_tuner tunerType;
    uint32_t sampleRate, centerFrequency;
    int ppm, directSamplingMode;
    size_t numBuffers, bufferLength;
    bool iqSwap, agcMode, offsetMode;
    double IFGain[6], tunerGain;

    std::vector<std::complex<float> > _lut_32f;
    std::vector<std::complex<float> > _lut_swap_32f;
    std::vector<std::complex<int16_t> > _lut_16i;
    std::vector<std::complex<int16_t> > _lut_swap_16i;

public:
    //async api usage
    std::thread _rx_async_thread;
    void rx_async_operation(void);
    void rx_callback(unsigned char *buf, uint32_t len);

    std::mutex _buf_mutex;
    std::condition_variable _buf_cond;

    std::vector<std::vector<signed char> > _buffs;
    size_t	_buf_head;
    size_t	_buf_tail;
    size_t	_buf_count;
    signed char *_currentBuff;
    bool _overflowEvent;
    size_t _currentHandle;
    size_t bufferedElems;
    bool resetBuffer;

    static int rtl_count;
    static std::vector<SoapySDR::Kwargs> rtl_devices;
    static double gainMin, gainMax;
};
