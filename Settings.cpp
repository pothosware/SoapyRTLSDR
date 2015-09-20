/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapyRTLSDR.hpp"

SoapyRTLSDR::SoapyRTLSDR(const SoapySDR::Kwargs &args)
{
    // create lookup tables
    for (unsigned int i = 0; i <= 0xffff; i++) {
# if (__BYTE_ORDER == __LITTLE_ENDIAN)
        std::complex<float> v32f, vs32f;

        v32f.real((float(i & 0xff) - 127.4f) * (1.0f/128.0f));
        v32f.imag((float(i >> 8) - 127.4f) * (1.0f/128.0f));
        _lut_32f.push_back(v32f);

        vs32f.real(v32f.imag());
        vs32f.imag(v32f.real());
        _lut_swap_32f.push_back(vs32f);


        // TODO: unsigned short

#else // BIG_ENDIAN
    #error  TODO
    //        tmp_swap.imag = tmp.real = (float(i >> 8) - 127.4f) * (1.0f/128.0f);
    //        tmp_swap.real = tmp.imag = (float(i & 0xff) - 127.4f) * (1.0f/128.0f);
    //        _lut.push_back(tmp);
    //        _lut_swap.push_back(tmp_swap);
#endif
    }

    offsetModeChanged = false;
    offsetMode = newOffsetMode = 0;
    iqSwap = false;
    iqSwapChanged = false;
    rxFormat = RTL_RX_FORMAT_FLOAT32;
    agcMode = newAgcMode = false;
    agcModeChanged = false;
    dev = NULL;
    directSamplingMode = newDirectSamplingMode = 0;
    directSamplingModeChanged = false;
    centerFrequency = newCenterFrequency = 100000000;
    centerFrequencyChanged = true;
    sampleRate = newSampleRate = 2048000;
    sampleRateChanged = true;
    bufferSize = 16384*6;
    deviceId = -1;
    ppm = newPpm = 0;
    ppmChanged = false;

    int rtl_count = rtlsdr_get_device_count();

    if (!rtl_count) {
        throw std::runtime_error("RTL-SDR device not found.");
    }

    deviceId = -1;

//    for (SoapySDR::Kwargs::const_iterator i = args.begin(); i != args.end(); i++) {
//        SoapySDR_logf(SOAPY_SDR_DEBUG, "\t [%s == %s]", i->first.c_str(), i->second.c_str());
//
//    }

    char manufact[256], product[256], serial[256];

    if (args.count("device_index") != 0) {
        int deviceId_in = std::stoi(args.at("device_index"));
        if (!std::isnan(deviceId_in)) {
            deviceId = deviceId_in;
        }
        if (deviceId < 0 && deviceId >= rtl_count) {
            throw std::runtime_error("device_index out of range [0 .. " + std::to_string(rtl_count) + "].");
        }

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Using device by parameter device_index = %d", deviceId_in);
    } else if (args.count("serial") != 0) {
        std::string deviceSerialFind = args.at("serial");

        for (int i = 0; i < rtl_count; i++) {
            if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) == 0) {
                std::string deviceName = std::string(rtlsdr_get_device_name(i)) + " :: " + std::string(serial);
                if (std::string(serial) == deviceSerialFind) {
                    SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device #%d by serial %s -- Manufacturer: %s, Product Name: %s, Serial: %s", i, deviceSerialFind.c_str(), manufact, product, serial);
                    deviceId = i;
                    break;
                }
            }
        }

        if (deviceId == -1) {
            throw std::runtime_error("Unable to find requested RTL-SDR device by serial.");
        }
    } else if (args.count("label") != 0) {
        std::string labelFind = args.at("label");
        for (int i = 0; i < rtl_count; i++) {
            if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) == 0) {
                std::string deviceName = std::string(rtlsdr_get_device_name(i)) + " :: " + std::string(serial);
                if (deviceName == labelFind) {
                    SoapySDR_logf(SOAPY_SDR_DEBUG, "Found RTL-SDR Device #%d by name: %s", deviceName.c_str());
                    deviceId = i;
                    break;
                }
            }
        }

        if (deviceId == -1) {
            throw std::runtime_error("Unable to find requested RTL-SDR device by label.");
        }
    }

    if (deviceId == -1) {
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Using first RTL-SDR Device #0: %s", rtlsdr_get_device_name(0));
        deviceId = 0;
    }
}

SoapyRTLSDR::~SoapyRTLSDR(void)
{
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

    args["direct_sampling_mode"] = std::to_string(directSamplingMode);
    args["offset_tuning"] = offsetMode?"1":"0";
    args["iq_swap"] = iqSwap?"1":"0";
    args["buffer_size"] = std::to_string(bufferSize);
    args["ppm"] = std::to_string(ppm);
    args["_help"] = "SoapyRTLSDR Driver\n Address:\t https://github.com/pothosware/SoapyRTLSDR\n\
 Buffer Size\t [buffer_size]: default " + std::to_string(16384*6) + " (16384*6)\n\
 Direct Sampling [direct_sampling_mode]: 0 = Off, 1 = I ADC, 2 = Q ADC\n\
 Offset Tuning\t [offset_tuning]: 0 = Off, 1 = On\n\
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
    /*
        int num_gains = rtlsdr_get_tuner_gains(dev, NULL);

        int *gains = (int *)malloc(sizeof(int) * num_gains);
        rtlsdr_get_tuner_gains(dev, gains);

        std::cout << "\t Valid gains: ";
        for (int g = 0; g < num_gains; g++) {
        if (g > 0) {
        std::cout << ", ";
        }
        std::cout << ((float)gains[g]/10.0f);
        }
        std::cout << std::endl;

        free(gains);
     */

    return results;
}

void SoapyRTLSDR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    agcMode = automatic;
    agcModeChanged = true;
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
    SoapySDR::Device::setGain(direction,channel,name,value);
}

double SoapyRTLSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    return SoapySDR::Device::getGain(direction,channel,name);
}

SoapySDR::Range SoapyRTLSDR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    return SoapySDR::Device::getGainRange(direction,channel,name);

    /*
        int num_gains = rtlsdr_get_tuner_gains(dev, NULL);

        int *gains = (int *)malloc(sizeof(int) * num_gains);
        rtlsdr_get_tuner_gains(dev, gains);

        std::cout << "\t Valid gains: ";
        for (int g = 0; g < num_gains; g++) {
        if (g > 0) {
        std::cout << ", ";
        }
        std::cout << ((float)gains[g]/10.0f);
        }
        std::cout << std::endl;

        free(gains);
     */
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyRTLSDR::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        newCenterFrequency = (uint32_t)frequency;
        centerFrequencyChanged = true;
    }
}

double SoapyRTLSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        if (centerFrequencyChanged) {
            return (double)newCenterFrequency;
        }
        return (double)centerFrequency;
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
    newSampleRate = rate;
    sampleRateChanged = true;
}

double SoapyRTLSDR::getSampleRate(const int direction, const size_t channel) const
{
    if (sampleRateChanged) {
        return newSampleRate;
    }
    
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
