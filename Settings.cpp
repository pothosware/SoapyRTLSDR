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

#include "SoapyRTLSDR.hpp"

int SoapyRTLSDR::rtl_count;
std::vector< SoapySDR::Kwargs > SoapyRTLSDR::rtl_devices;
double SoapyRTLSDR::gainMin;
double SoapyRTLSDR::gainMax;


SoapyRTLSDR::SoapyRTLSDR(const SoapySDR::Kwargs &args)
{
    offsetMode = 0;
    iqSwap = false;
    rxFormat = RTL_RX_FORMAT_FLOAT32;
    agcMode = false;
    dev = NULL;
    directSamplingMode = 0;
    centerFrequency = 100000000;
    sampleRate = 2048000;
    bufferLength = DEFAULT_BUFFER_LENGTH;
    numBuffers = DEFAULT_NUM_BUFFERS;
    bufferSize = bufferLength*numBuffers;
    deviceId = -1;
    ppm = 0;
    IFGain = 0;
    tunerGain = 0;
    resetBuffer = false;

    if (!SoapyRTLSDR::rtl_count) {
        throw std::runtime_error("RTL-SDR device not found.");
    }

    deviceId = -1;

    //    for (SoapySDR::Kwargs::const_iterator i = args.begin(); i != args.end(); i++) {
    //        SoapySDR_logf(SOAPY_SDR_DEBUG, "\t [%s == %s]", i->first.c_str(), i->second.c_str());
    //    }

    if (args.count("rtl") != 0) {
        int deviceId_in = std::stoi(args.at("rtl"));
        if (!std::isnan(deviceId_in)) {
            deviceId = deviceId_in;
        }
        if (deviceId < 0 || deviceId >= SoapyRTLSDR::rtl_count) {
            throw std::runtime_error("device index 'rtl' out of range [0 .. " + std::to_string(SoapyRTLSDR::rtl_count) + "].");
        }

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device using device index parameter 'rtl' = %d", deviceId);
    } else if (args.count("serial") != 0) {
        std::string deviceSerialFind = args.at("serial");

        for (int i = 0; i < SoapyRTLSDR::rtl_count; i++) {
            SoapySDR::Kwargs devInfo = SoapyRTLSDR::rtl_devices[i];
            if (devInfo.at("serial") == deviceSerialFind) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device #%d by serial %s -- Manufacturer: %s, Product Name: %s, Serial: %s", i, deviceSerialFind.c_str(), devInfo.at("manufacturer").c_str(), devInfo.at("product").c_str(), devInfo.at("serial").c_str());
                deviceId = i;
                break;
            }
        }
    } else if (args.count("label") != 0) {
        std::string labelFind = args.at("label");
        for (int i = 0; i < SoapyRTLSDR::rtl_count; i++) {
            SoapySDR::Kwargs devInfo = SoapyRTLSDR::rtl_devices[i];
            if (devInfo.at("label") == labelFind) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device #%d by name: %s", devInfo.at("label").c_str());
                deviceId = i;
                break;
            }
        }
    }

    if (deviceId == -1) {
        throw std::runtime_error("Unable to find requested RTL-SDR device.");
    }

    if (deviceId == -1) {
        deviceId = 0;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Using first RTL-SDR Device #0: %s", SoapyRTLSDR::rtl_devices[deviceId].at("label").c_str());
    }

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR opening device %d", deviceId);
    rtlsdr_open(&dev, deviceId);

    if (args.count("buflen") != 0) {
        int bufferLength_in = std::stoi(args.at("buflen"));
        if (!std::isnan(bufferLength_in) && bufferLength_in) {
            bufferLength = bufferLength_in;
        }
    } else {
        bufferLength = DEFAULT_BUFFER_LENGTH;
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using buffer length %d", bufferLength);

    if (args.count("buffers") != 0) {
        int numBuffers_in = std::stoi(args.at("buffers"));
        if (!std::isnan(numBuffers_in) && numBuffers_in) {
            numBuffers = numBuffers_in;
        }
    } else {
        numBuffers = DEFAULT_NUM_BUFFERS;
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using %d buffers", numBuffers);

    if (args.count("direct_samp") != 0) {
        int directSamplingMode_in = std::stoi(args.at("direct_samp"));
        if (!std::isnan(directSamplingMode_in)) {
            directSamplingMode = directSamplingMode_in;
        }
    }

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode %d", directSamplingMode);


    if (args.count("iq_swap") != 0) {
        int iqSwap_in = std::stoi(args.at("iq_swap"));
        if (!std::isnan(iqSwap_in)) {
            iqSwap = iqSwap_in?true:false;
        }
    } else {
        iqSwap = 0;
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap?"Yes":"No");

    if (args.count("offset_tune") != 0) {
        int offsetMode_in = std::stoi(args.at("offset_tune"));
        if (!std::isnan(offsetMode_in)) {
            offsetMode = offsetMode_in?true:false;
        }
    } else {
        offsetMode = false;
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode?"Yes":"No");
    rtlsdr_set_offset_tuning(dev, offsetMode?1:0);

    if (args.count("ppm") != 0) {
        int ppm_in = std::stoi(args.at("ppm"));
        if (!std::isnan(ppm_in)) {
            ppm = ppm_in;
        }
    } else {
        ppm = 0;
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR PPM: %d", ppm);

}

SoapyRTLSDR::~SoapyRTLSDR(void)
{
    rtlsdr_close(dev);
    //cleanup device handles
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyRTLSDR::getDriverKey(void) const
{
    return "RTLSDR";
}

std::string SoapyRTLSDR::getHardwareKey(void) const
{
    return "RTLSDR";
}

SoapySDR::Kwargs SoapyRTLSDR::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["rtl"] =
            args["direct_samp"] = std::to_string(directSamplingMode);
    args["offset_tune"] = offsetMode?"1":"0";
    args["iq_swap"] = iqSwap?"1":"0";
    args["num_buffers"] = std::to_string(numBuffers);
    args["buflen"] = std::to_string(bufferLength);
    args["ppm"] = std::to_string(ppm);
    args["_help"] = "SoapyRTLSDR Driver\n Address:\t https://github.com/pothosware/SoapyRTLSDR\n\
 Buffer Size\t [bufflen]: default " + std::to_string(16384) + "\n\
 Buffer Count\t [buffers]: default " + std::to_string(16) + "\n\
 Direct Sampling [direct_samp]: 0 = Off, 1 = I ADC, 2 = Q ADC\n\
 Offset Tuning\t [offset_tune]: 0 = Off, 1 = On\n\
 Swap I/Q\t [iq_swap]: 0 = Off, 1 = On\n\
 PPM Offset\t [ppm]: Default 0 (parts per million)\n";

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyRTLSDR::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX)?1:0;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyRTLSDR::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> antennas;
    antennas.push_back("RX");
    return antennas;
}

void SoapyRTLSDR::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("setAntena failed: RTL-SDR only supports RX");
    }
}

std::string SoapyRTLSDR::getAntenna(const int direction, const size_t channel) const
{
    return "RX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/


bool SoapyRTLSDR::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return false;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyRTLSDR::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IF");
    results.push_back("TUNER");

    return results;
}

void SoapyRTLSDR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    agcMode = automatic;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR AGC: %s", automatic?"Automatic":"Manual");
    rtlsdr_set_agc_mode(dev, agcMode?1:0);
}

bool SoapyRTLSDR::getGainMode(const int direction, const size_t channel) const
{
    return SoapySDR::Device::getGainMode(direction, channel);
}

void SoapyRTLSDR::setGain(const int direction, const size_t channel, const double value)
{
    //set the overall gain by distributing it across available gain elements
    //OR delete this function to use SoapySDR's default gain distribution algorithm...
    SoapySDR::Device::setGain(direction,channel,value);
}

void SoapyRTLSDR::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    if (name == "IF") {
        IFGain = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain: %f", IFGain);
        rtlsdr_set_tuner_if_gain(dev, 0, (int)IFGain*10.0);
    }

    if (name == "TUNER") {
        tunerGain = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR Tuner Gain: %f", tunerGain);
        rtlsdr_set_tuner_gain(dev, (int)tunerGain*10.0);
    }
}

double SoapyRTLSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "IF") {
        return IFGain;
    }

    if (name == "TUNER") {
        return tunerGain;
    }

    return 0;
}

SoapySDR::Range SoapyRTLSDR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    return SoapySDR::Range(SoapyRTLSDR::gainMin, SoapyRTLSDR::gainMax);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyRTLSDR::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        centerFrequency = (uint32_t)frequency;
        resetBuffer = true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", centerFrequency);
        rtlsdr_set_center_freq(dev, centerFrequency);
    }

    if (name == "CORR") {
        ppm = (int)frequency;
        rtlsdr_set_freq_correction(dev, ppm);
    }
}

double SoapyRTLSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return (double)centerFrequency;
    }

    if (name == "CORR")
    {
        return (double)ppm;
    }

    return 0;
}

std::vector<std::string> SoapyRTLSDR::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapyRTLSDR::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        SoapySDR::Range rfRange(27000000,1764000000);
        results.push_back(rfRange);
    }
    return results;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyRTLSDR::setSampleRate(const int direction, const size_t channel, const double rate)
{
    sampleRate = rate;
    resetBuffer = true;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
    rtlsdr_set_sample_rate(dev, sampleRate);
}

double SoapyRTLSDR::getSampleRate(const int direction, const size_t channel) const
{
    return sampleRate;
}

std::vector<double> SoapyRTLSDR::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> results;

    results.push_back(250000);
    results.push_back(1024000);
    results.push_back(1536000);
    results.push_back(1792000);
    results.push_back(1920000);
    results.push_back(2048000);
    results.push_back(2160000);
    results.push_back(2560000);
    results.push_back(2880000);
    results.push_back(3200000);

    return results;
}

void SoapyRTLSDR::setBandwidth(const int direction, const size_t channel, const double bw)
{
    SoapySDR::Device::setBandwidth(direction, channel, bw);
}

double SoapyRTLSDR::getBandwidth(const int direction, const size_t channel) const
{
    return SoapySDR::Device::getBandwidth(direction, channel);
}

std::vector<double> SoapyRTLSDR::listBandwidths(const int direction, const size_t channel) const
{
    std::vector<double> results;

    return results;
}
