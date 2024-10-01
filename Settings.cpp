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
#include <SoapySDR/Time.hpp>
#include <algorithm>
#include <cstring>

SoapyRTLSDR::SoapyRTLSDR(const SoapySDR::Kwargs &args):
    deviceId(-1),
    dev(nullptr),
    rxFormat(RTL_RX_FORMAT_FLOAT32),
    tunerType(RTLSDR_TUNER_R820T),
    sampleRate(2048000),
    centerFrequency(100000000),
    bandwidth(0),
    ppm(0),
    directSamplingMode(0),
    numBuffers(DEFAULT_NUM_BUFFERS),
    bufferLength(DEFAULT_BUFFER_LENGTH),
    iqSwap(false),
    gainMode(false),
    offsetMode(false),
    digitalAGC(false),
    testMode(false),
#if HAS_RTLSDR_SET_BIAS_TEE
    biasTee(false),
#endif
#if HAS_RTLSDR_SET_DITHERING
    dithering(true),
#endif
    tunerGain(0.0),
    ticks(false),
    bufferedElems(0),
    resetBuffer(false),
    gainMin(0.0),
    gainMax(0.0)
{
    if (args.count("label") != 0) SoapySDR_logf(SOAPY_SDR_INFO, "Opening %s...", args.at("label").c_str());

    //if a serial is not present, then findRTLSDR had zero devices enumerated
    if (args.count("serial") == 0) throw std::runtime_error("No RTL-SDR devices found!");

    const auto serial = args.at("serial");
    deviceId = rtlsdr_get_index_by_serial(serial.c_str());
    if (deviceId < 0) throw std::runtime_error("rtlsdr_get_index_by_serial("+serial+") - " + std::to_string(deviceId));

    if (args.count("tuner") != 0) tunerType = rtlStringToTuner(args.at("tuner"));
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Tuner type: %s", rtlTunerToString(tunerType).c_str());

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR opening device %d", deviceId);
    if (rtlsdr_open(&dev, deviceId) != 0) {
        throw std::runtime_error("Unable to open RTL-SDR device");
    }

    //extract min/max overall gain range
    int num_gains = rtlsdr_get_tuner_gains(dev, nullptr);
    if (num_gains > 0)
    {
        std::vector<int> gains(num_gains);
        rtlsdr_get_tuner_gains(dev, gains.data());
        gainMin = *std::min_element(gains.begin(), gains.end()) / 10.0;
        gainMax = *std::max_element(gains.begin(), gains.end()) / 10.0;
    }
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
    args["index"] = std::to_string(deviceId);

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyRTLSDR::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapyRTLSDR::getFullDuplex(const int direction, const size_t channel) const
{
    return false;
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
    int r = rtlsdr_set_freq_correction(dev, int(value));
    if (r == -2)
    {
        return; // CORR didn't actually change, we are done
    }
    if (r != 0)
    {
        throw std::runtime_error("setFrequencyCorrection failed");
    }
    ppm = rtlsdr_get_freq_correction(dev);
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

        return SoapySDR::Range(gainMin, gainMax);
    } else {
        return SoapySDR::Range(gainMin, gainMax);
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
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", (uint32_t)frequency);
        int r = rtlsdr_set_center_freq(dev, (uint32_t)frequency);
        if (r != 0)
        {
            throw std::runtime_error("setFrequency failed");
        }
        centerFrequency = rtlsdr_get_center_freq(dev);
    }

    if (name == "CORR")
    {
        int r = rtlsdr_set_freq_correction(dev, (int)frequency);
        if (r == -2)
        {
            return; // CORR didn't actually change, we are done
        }
        if (r != 0)
        {
            throw std::runtime_error("setFrequencyCorrection failed");
        }
        ppm = rtlsdr_get_freq_correction(dev);
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
    char manufact[256] = {0};
    char product[256] = {0};

    // Get manufact and product USB strings to detect RTL-SDR Blog V4 model
    rtlsdr_get_usb_strings(dev, manufact, product, NULL);

    if (name == "RF")
    {
        if (tunerType == RTLSDR_TUNER_E4000) {
            results.push_back(SoapySDR::Range(52000000, 2200000000));
        } else if (tunerType == RTLSDR_TUNER_FC0012) {
            results.push_back(SoapySDR::Range(22000000, 1100000000));
        } else if (tunerType == RTLSDR_TUNER_FC0013) {
            results.push_back(SoapySDR::Range(22000000, 948600000));
        // The RTL-SDR Blog V4 can tune down to 0 MHz (in reality ~300 kHz) because of the built in upconverter.
        } else if (tunerType == RTLSDR_TUNER_R828D && strcmp(manufact, "RTLSDRBlog") == 0 && strcmp(product, "Blog V4") == 0) {
            results.push_back(SoapySDR::Range(0, 1764000000));
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
    long long ns = SoapySDR::ticksToTimeNs(ticks, sampleRate);
    sampleRate = rate;
    resetBuffer = true;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
    int r = rtlsdr_set_sample_rate(dev, sampleRate);
    if (r == -EINVAL)
    {
        throw std::runtime_error("setSampleRate failed: RTL-SDR does not support this sample rate");
    }
    if (r != 0)
    {
        throw std::runtime_error("setSampleRate failed");
    }
    sampleRate = rtlsdr_get_sample_rate(dev);
    ticks = SoapySDR::timeNsToTicks(ns, sampleRate);
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

SoapySDR::RangeList SoapyRTLSDR::getSampleRateRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;

    results.push_back(SoapySDR::Range(225001, 300000));
    results.push_back(SoapySDR::Range(900001, 3200000));

    return results;
}

void SoapyRTLSDR::setBandwidth(const int direction, const size_t channel, const double bw)
{
    int r = rtlsdr_set_tuner_bandwidth(dev, bw);
    if (r != 0)
    {
        throw std::runtime_error("setBandwidth failed");
    }
    bandwidth = bw;
}

double SoapyRTLSDR::getBandwidth(const int direction, const size_t channel) const
{
    if (bandwidth == 0) // auto / full bandwidth
        return sampleRate;
    return bandwidth;
}

std::vector<double> SoapyRTLSDR::listBandwidths(const int direction, const size_t channel) const
{
    std::vector<double> results;

    return results;
}

SoapySDR::RangeList SoapyRTLSDR::getBandwidthRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;

    // stub, not sure what the sensible ranges for different tuners are.
    results.push_back(SoapySDR::Range(0, 8000000));

    return results;
}

/*******************************************************************
 * Time API
 ******************************************************************/

std::vector<std::string> SoapyRTLSDR::listTimeSources(void) const
{
    std::vector<std::string> results;

    results.push_back("sw_ticks");

    return results;
}

std::string SoapyRTLSDR::getTimeSource(void) const
{
    return "sw_ticks";
}

bool SoapyRTLSDR::hasHardwareTime(const std::string &what) const
{
    return what == "" || what == "sw_ticks";
}

long long SoapyRTLSDR::getHardwareTime(const std::string &what) const
{
    return SoapySDR::ticksToTimeNs(ticks, sampleRate);
}

void SoapyRTLSDR::setHardwareTime(const long long timeNs, const std::string &what)
{
    ticks = SoapySDR::timeNsToTicks(timeNs, sampleRate);
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

    SoapySDR::ArgInfo testModeArg;

    testModeArg.key = "testmode";
    testModeArg.value = "false";
    testModeArg.name = "Test Mode";
    testModeArg.description = "RTL-SDR Test Mode";
    testModeArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(testModeArg);

#if HAS_RTLSDR_SET_BIAS_TEE
    SoapySDR::ArgInfo biasTeeArg;

    biasTeeArg.key = "biastee";
    biasTeeArg.value = "false";
    biasTeeArg.name = "Bias Tee";
    biasTeeArg.description = "RTL-SDR Blog V.3 Bias-Tee Mode";
    biasTeeArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(biasTeeArg);
#endif

#if HAS_RTLSDR_SET_DITHERING
    SoapySDR::ArgInfo ditheringArg;

    ditheringArg.key = "dithering";
    ditheringArg.value = "true";
    ditheringArg.name = "Dithering";
    ditheringArg.description = "RTL-SDR Dithering Mode";
    ditheringArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(ditheringArg);
#endif

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
    else if (key == "testmode")
    {
        testMode = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR test mode: %s", testMode ? "true" : "false");
        rtlsdr_set_testmode(dev, testMode ? 1 : 0);
    }
#if HAS_RTLSDR_SET_BIAS_TEE
    else if (key == "biastee")
    {
        biasTee = (value == "true") ? true: false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR bias tee mode: %s", biasTee ? "true" : "false");
        rtlsdr_set_bias_tee(dev, biasTee ? 1 : 0);
    }
#endif
#if HAS_RTLSDR_SET_DITHERING
    else if (key == "dithering")
    {
        dithering = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR dithering mode: %s", dithering ? "true" : "false");
        rtlsdr_set_dithering(dev, dithering ? 1 : 0);
    }
#endif
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
    } else if (key == "testmode") {
        return testMode?"true":"false";
#if HAS_RTLSDR_SET_BIAS_TEE
    } else if (key == "biastee") {
        return biasTee?"true":"false";
#endif
#if HAS_RTLSDR_SET_DITHERING
    } else if (key == "dithering") {
        return dithering?"true":"false";
#endif
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

