/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2015-2017 Josh Blum

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
std::vector<SoapySDR::Kwargs> SoapyRTLSDR::rtl_devices;
double SoapyRTLSDR::gainMin;
double SoapyRTLSDR::gainMax;

SoapyRTLSDR::SoapyRTLSDR(const SoapySDR::Kwargs &args)
{
    if (!SoapyRTLSDR::rtl_count)
    {
        throw std::runtime_error("RTL-SDR device not found.");
    }

    deviceId = -1;
    dev = NULL;

    rxFormat = RTL_RX_FORMAT_FLOAT32;
    tunerType = RTLSDR_TUNER_R820T;

    sampleRate = 2048000;
    centerFrequency = 100000000;

    ppm = 0;
    directSamplingMode = 0;
    numBuffers = DEFAULT_NUM_BUFFERS;
    bufferLength = DEFAULT_BUFFER_LENGTH;

    iqSwap = false;
    gainMode = false;
    offsetMode = false;
    digitalAGC = false;

    bufferedElems = 0;
    resetBuffer = false;

    if (args.count("rtl") != 0)
    {
        try
        {
            deviceId = std::stoi(args.at("rtl"));
        }
        catch (const std::invalid_argument &)
        {
        }
        if (deviceId < 0 || deviceId >= SoapyRTLSDR::rtl_count)
        {
            throw std::runtime_error(
                    "device index 'rtl' out of range [0 .. " + std::to_string(SoapyRTLSDR::rtl_count) + "].");
        }

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device using device index parameter 'rtl' = %d", deviceId);
    }
    else if (args.count("serial") != 0)
    {
        std::string deviceSerialFind = args.at("serial");

        for (int i = 0; i < SoapyRTLSDR::rtl_count; i++)
        {
            SoapySDR::Kwargs devInfo = SoapyRTLSDR::rtl_devices[i];
            if (devInfo.at("serial") == deviceSerialFind)
            {
                SoapySDR_logf(SOAPY_SDR_DEBUG,
                        "Found RTL-SDR Device #%d by serial %s -- Manufacturer: %s, Product Name: %s, Serial: %s", i,
                        deviceSerialFind.c_str(), devInfo.at("manufacturer").c_str(), devInfo.at("product").c_str(),
                        devInfo.at("serial").c_str());
                deviceId = i;
                break;
            }
        }
    }
    else if (args.count("label") != 0)
    {
        std::string labelFind = args.at("label");
        for (int i = 0; i < SoapyRTLSDR::rtl_count; i++)
        {
            SoapySDR::Kwargs devInfo = SoapyRTLSDR::rtl_devices[i];
            if (devInfo.at("label") == labelFind)
            {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device #%d by name: %s", devInfo.at("label").c_str());
                deviceId = i;
                break;
            }
        }
    }

    if (deviceId == -1)
    {
        throw std::runtime_error("Unable to find requested RTL-SDR device.");
    }

    if (args.count("tuner") != 0)
    {
        tunerType = rtlStringToTuner(args.at("tuner"));
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Tuner type: %s", rtlTunerToString(tunerType).c_str());

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR opening device %d", deviceId);

    rtlsdr_open(&dev, deviceId);
}

SoapyRTLSDR::~SoapyRTLSDR(void)
{
    //cleanup device handles
    rtlsdr_close(dev);
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
    switch (rtlsdr_get_tuner_type(dev))
    {
    case RTLSDR_TUNER_UNKNOWN:
        return "UNKNOWN";
    case RTLSDR_TUNER_E4000:
        return "E4000";
    case RTLSDR_TUNER_FC0012:
        return "FC0012";
    case RTLSDR_TUNER_FC0013:
        return "FC0013";
    case RTLSDR_TUNER_FC2580:
        return "FC2580";
    case RTLSDR_TUNER_R820T:
        return "R820T";
    case RTLSDR_TUNER_R828D:
        return "R828D";
    default:
        return "OTHER";
    }
}

SoapySDR::Kwargs SoapyRTLSDR::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/pothosware/SoapyRTLSDR";
    args["rtl"] = std::to_string(deviceId);

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyRTLSDR::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
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

bool SoapyRTLSDR::hasFrequencyCorrection(const int direction, const size_t channel) const
{
    return true;
}

void SoapyRTLSDR::setFrequencyCorrection(const int direction, const size_t channel, const double value)
{
    ppm = int(value);
    rtlsdr_set_freq_correction(dev, ppm);
}

double SoapyRTLSDR::getFrequencyCorrection(const int direction, const size_t channel) const
{
    return double(ppm);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyRTLSDR::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    if (tunerType == RTLSDR_TUNER_E4000)
    {
        results.push_back("IF1");
        results.push_back("IF2");
        results.push_back("IF3");
        results.push_back("IF4");
        results.push_back("IF5");
        results.push_back("IF6");
    }
    results.push_back("TUNER");

    return results;
}

bool SoapyRTLSDR::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapyRTLSDR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    gainMode = automatic;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR gain mode: %s", automatic ? "Automatic" : "Manual");
    rtlsdr_set_tuner_gain_mode(dev, gainMode ? 0 : 1);
}

bool SoapyRTLSDR::getGainMode(const int direction, const size_t channel) const
{
    return gainMode;
}

void SoapyRTLSDR::setGain(const int direction, const size_t channel, const double value)
{
    //set the overall gain by distributing it across available gain elements
    //OR delete this function to use SoapySDR's default gain distribution algorithm...
    SoapySDR::Device::setGain(direction, channel, value);
}

void SoapyRTLSDR::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    {
        int stage = 1;
        if (name.length() > 2)
        {
            int stage_in = name.at(2) - '0';
            if ((stage_in < 1) || (stage_in > 6))
            {
                throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
            }
        }
        if (tunerType == RTLSDR_TUNER_E4000) {
            IFGain[stage - 1] = getE4000Gain(stage, (int)value);
        } else {
            IFGain[stage - 1] = value;
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain for stage %d: %f", stage, IFGain[stage - 1]);
        rtlsdr_set_tuner_if_gain(dev, stage, (int) IFGain[stage - 1] * 10.0);
    }

    if (name == "TUNER")
    {
        tunerGain = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR Tuner Gain: %f", tunerGain);
        rtlsdr_set_tuner_gain(dev, (int) tunerGain * 10.0);
    }
}

double SoapyRTLSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    {
        int stage = 1;
        if (name.length() > 2)
        {
            int stage_in = name.at(2) - '0';
            if ((stage_in < 1) || (stage_in > 6))
            {
                throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
            } else {
                stage = stage_in;
            }
        }
        if (tunerType == RTLSDR_TUNER_E4000) {
            return getE4000Gain(stage, IFGain[stage - 1]);
        }

        return IFGain[stage - 1];
    }

    if (name == "TUNER")
    {
        return tunerGain;
    }

    return 0;
}

SoapySDR::Range SoapyRTLSDR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (tunerType == RTLSDR_TUNER_E4000 && name != "TUNER") {
        if (name == "IF1") {
            return SoapySDR::Range(-3, 6);
        }
        if (name == "IF2" || name == "IF3") {
            return SoapySDR::Range(0, 9);
        }
        if (name == "IF4") {
            return SoapySDR::Range(0, 2);
        }
        if (name == "IF5" || name == "IF6") {
            return SoapySDR::Range(3, 15);
        }

        return SoapySDR::Range(SoapyRTLSDR::gainMin, SoapyRTLSDR::gainMax);
    } else {
        return SoapySDR::Range(SoapyRTLSDR::gainMin, SoapyRTLSDR::gainMax);
    }
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyRTLSDR::setFrequency(
        const int direction,
        const size_t channel,
        const std::string &name,
        const double frequency,
        const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        centerFrequency = (uint32_t) frequency;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", centerFrequency);
        rtlsdr_set_center_freq(dev, centerFrequency);
    }

    if (name == "CORR")
    {
        ppm = (int) frequency;
        rtlsdr_set_freq_correction(dev, ppm);
    }
}

double SoapyRTLSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return (double) centerFrequency;
    }

    if (name == "CORR")
    {
        return (double) ppm;
    }

    return 0;
}

std::vector<std::string> SoapyRTLSDR::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    names.push_back("CORR");
    return names;
}

SoapySDR::RangeList SoapyRTLSDR::getFrequencyRange(
        const int direction,
        const size_t channel,
        const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        if (tunerType == RTLSDR_TUNER_E4000) {
            results.push_back(SoapySDR::Range(52000000, 2200000000));
        } else if (tunerType == RTLSDR_TUNER_FC0012) {
            results.push_back(SoapySDR::Range(22000000, 1100000000));
        } else if (tunerType == RTLSDR_TUNER_FC0013) {
            results.push_back(SoapySDR::Range(22000000, 948600000));
        } else {
            results.push_back(SoapySDR::Range(24000000, 1764000000));
        }
    }
    if (name == "CORR")
    {
        results.push_back(SoapySDR::Range(-1000, 1000));
    }
    return results;
}

SoapySDR::ArgInfoList SoapyRTLSDR::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
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

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyRTLSDR::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;

    SoapySDR::ArgInfo directSampArg;

    directSampArg.key = "direct_samp";
    directSampArg.value = "0";
    directSampArg.name = "Direct Sampling";
    directSampArg.description = "RTL-SDR Direct Sampling Mode";
    directSampArg.type = SoapySDR::ArgInfo::STRING;
    directSampArg.options.push_back("0");
    directSampArg.optionNames.push_back("Off");
    directSampArg.options.push_back("1");
    directSampArg.optionNames.push_back("I-ADC");
    directSampArg.options.push_back("2");
    directSampArg.optionNames.push_back("Q-ADC");

    setArgs.push_back(directSampArg);

    SoapySDR::ArgInfo offsetTuneArg;

    offsetTuneArg.key = "offset_tune";
    offsetTuneArg.value = "false";
    offsetTuneArg.name = "Offset Tune";
    offsetTuneArg.description = "RTL-SDR Offset Tuning Mode";
    offsetTuneArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(offsetTuneArg);

    SoapySDR::ArgInfo iqSwapArg;

    iqSwapArg.key = "iq_swap";
    iqSwapArg.value = "false";
    iqSwapArg.name = "I/Q Swap";
    iqSwapArg.description = "RTL-SDR I/Q Swap Mode";
    iqSwapArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(iqSwapArg);

    SoapySDR::ArgInfo digitalAGCArg;

    digitalAGCArg.key = "digital_agc";
    digitalAGCArg.value = "false";
    digitalAGCArg.name = "Digital AGC";
    digitalAGCArg.description = "RTL-SDR digital AGC Mode";
    digitalAGCArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(digitalAGCArg);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "SETARGS?");

    return setArgs;
}

void SoapyRTLSDR::writeSetting(const std::string &key, const std::string &value)
{
    if (key == "direct_samp")
    {
        try
        {
            directSamplingMode = std::stoi(value);
        }
        catch (const std::invalid_argument &) {
            SoapySDR_logf(SOAPY_SDR_ERROR, "RTL-SDR invalid direct sampling mode '%s', [0:Off, 1:I-ADC, 2:Q-ADC]", value.c_str());
            directSamplingMode = 0;
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode: %d", directSamplingMode);
        rtlsdr_set_direct_sampling(dev, directSamplingMode);
    }
    else if (key == "iq_swap")
    {
        iqSwap = ((value=="true") ? true : false);
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap ? "true" : "false");
    }
    else if (key == "offset_tune")
    {
        offsetMode = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode ? "true" : "false");
        rtlsdr_set_offset_tuning(dev, offsetMode ? 1 : 0);
    }
    else if (key == "digital_agc")
    {
        digitalAGC = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR digital agc mode: %s", digitalAGC ? "true" : "false");
        rtlsdr_set_agc_mode(dev, digitalAGC ? 1 : 0);
    }
}

std::string SoapyRTLSDR::readSetting(const std::string &key) const
{
    if (key == "direct_samp") {
        return std::to_string(directSamplingMode);
    } else if (key == "iq_swap") {
        return iqSwap?"true":"false";
    } else if (key == "offset_tune") {
        return offsetMode?"true":"false";
    } else if (key == "digital_agc") {
        return digitalAGC?"true":"false";
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}

std::string SoapyRTLSDR::rtlTunerToString(rtlsdr_tuner tunerType)
{
    std::string deviceTuner;
    switch (tunerType)
    {
    case RTLSDR_TUNER_UNKNOWN:
        deviceTuner = "Unknown";
        break;
    case RTLSDR_TUNER_E4000:
        deviceTuner = "Elonics E4000";
        break;
    case RTLSDR_TUNER_FC0012:
        deviceTuner = "Fitipower FC0012";
        break;
    case RTLSDR_TUNER_FC0013:
        deviceTuner = "Fitipower FC0013";
        break;
    case RTLSDR_TUNER_FC2580:
        deviceTuner = "Fitipower FC2580";
        break;
    case RTLSDR_TUNER_R820T:
        deviceTuner = "Rafael Micro R820T";
        break;
    case RTLSDR_TUNER_R828D:
        deviceTuner = "Rafael Micro R828D";
        break;
    default:
        deviceTuner = "Unknown";
    }
    return deviceTuner;
}

int SoapyRTLSDR::getE4000Gain(int stage, int gain) {
    static const int8_t if_stage1_gain[] = {
            -3, 6
    };

    static const int8_t if_stage23_gain[] = {
            0, 3, 6, 9
    };

    static const int8_t if_stage4_gain[] = {
            0, 1, 2 //, 2
    };

    static const int8_t if_stage56_gain[] = {
            3, 6, 9, 12, 15 // , 15, 15, 15 // wat?
    };

    const int8_t *if_stage = nullptr;
    int n_gains = 0;

    if (stage == 1) {
        if_stage = if_stage1_gain;
        n_gains = 2;
    } else if (stage == 2 || stage == 3) {
        if_stage = if_stage23_gain;
        n_gains = 4;
    } else if (stage == 4) {
        if_stage = if_stage4_gain;
        n_gains = 3;
    } else if (stage == 5 || stage == 6) {
        if_stage = if_stage56_gain;
        n_gains = 5;
    }

    if (n_gains && if_stage) {
        int gainMin = if_stage[0];
        int gainMax = if_stage[n_gains-1];

        if (gain > gainMax) {
            gain = gainMax;
        }

        if (gain < gainMin) {
            gain = gainMin;
        }

        for (int i = 0; i < n_gains-1; i++) {
            if (gain >= if_stage[i] && gain <= if_stage[i+1]) {
                gain = ((gain-if_stage[i]) < (if_stage[i+1]-gain))?if_stage[i]:if_stage[i+1];
            }
        }
    }

    return gain;
}


rtlsdr_tuner SoapyRTLSDR::rtlStringToTuner(std::string tunerType)
{
    rtlsdr_tuner deviceTuner = RTLSDR_TUNER_UNKNOWN;

    deviceTuner = RTLSDR_TUNER_UNKNOWN;

    if (tunerType == "Elonics E4000")
        deviceTuner = RTLSDR_TUNER_E4000;
    if (tunerType == "Fitipower FC0012")
        deviceTuner = RTLSDR_TUNER_FC0012;
    if (tunerType == "Fitipower FC0013")
        deviceTuner = RTLSDR_TUNER_FC0013;
    if (tunerType == "Fitipower FC2580")
        deviceTuner = RTLSDR_TUNER_FC2580;
    if (tunerType == "Rafael Micro R820T")
        deviceTuner = RTLSDR_TUNER_R820T;
    if (tunerType == "Rafael Micro R828D")
        deviceTuner = RTLSDR_TUNER_R828D;

    return deviceTuner;
}

