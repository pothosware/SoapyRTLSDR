/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapyRTLSDR.hpp"


int SoapyRTLSDR::rtl_count;
std::vector< SoapySDR::Kwargs > SoapyRTLSDR::rtl_devices;
double SoapyRTLSDR::gainMin;
double SoapyRTLSDR::gainMax;


SoapyRTLSDR::SoapyRTLSDR(const SoapySDR::Kwargs &args)
{
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
    bufferLength = DEFAULT_BUFFER_LENGTH;
    numBuffers = DEFAULT_NUM_BUFFERS;
    bufferSize = bufferLength*numBuffers;
    deviceId = -1;
    ppm = newPpm = 0;
    ppmChanged = false;
    IFGainChanged = false;
    IFGain = newIFGain = 0;
    tunerGainChanged = false;
    tunerGain = newTunerGain = 0;

    if (!SoapyRTLSDR::rtl_count) {
        throw std::runtime_error("RTL-SDR device not found.");
    }

    deviceId = -1;

    for (SoapySDR::Kwargs::const_iterator i = args.begin(); i != args.end(); i++) {
        SoapySDR_logf(SOAPY_SDR_DEBUG, "\t [%s == %s]", i->first.c_str(), i->second.c_str());

    }

    if (args.count("rtl") != 0) {
        int deviceId_in = std::stoi(args.at("rtl"));
        if (!std::isnan(deviceId_in)) {
            deviceId = deviceId_in;
        }
        if (deviceId < 0 && deviceId >= SoapyRTLSDR::rtl_count) {
            throw std::runtime_error("rtl out of range [0 .. " + std::to_string(SoapyRTLSDR::rtl_count) + "].");
        }

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Using device by parameter rtl = %d", deviceId);
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

        if (deviceId == -1) {
            throw std::runtime_error("Unable to find requested RTL-SDR device by serial.");
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

        if (deviceId == -1) {
            throw std::runtime_error("Unable to find requested RTL-SDR device by label.");
        }
    }

    if (deviceId == -1) {
        deviceId = 0;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Using first RTL-SDR Device #0: %s", SoapyRTLSDR::rtl_devices[deviceId].at("label").c_str());
    }

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
             if (directSamplingMode_in >= 0 && directSamplingMode_in <= 2) {
                 newDirectSamplingMode = directSamplingMode_in;
             } else {
                 throw std::runtime_error("direct_samp " + std::to_string(directSamplingMode_in) + " invalid.  0 = off, 1 = I ADC, 2 = Q ADC");
             }
         }
     } else {
         newDirectSamplingMode = 0;
     }
     directSamplingModeChanged = true;
     SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode %d", newDirectSamplingMode);


     if (args.count("iq_swap") != 0) {
         int iqSwap_in = std::stoi(args.at("iq_swap"));
         if (!std::isnan(iqSwap_in)) {
             iqSwap = iqSwap_in?true:false;
         }
     } else {
         iqSwap = 0;
     }
     iqSwapChanged = false;
     SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap?"Yes":"No");

     if (args.count("offset_tune") != 0) {
          int offsetMode_in = std::stoi(args.at("offset_tune"));
          if (!std::isnan(offsetMode_in)) {
              newOffsetMode = offsetMode_in?true:false;
          }
      } else {
          newOffsetMode = false;
      }
     offsetModeChanged = true;
     SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode?"Yes":"No");

     if (args.count("ppm") != 0) {
         int ppm_in = std::stoi(args.at("ppm"));
         if (!std::isnan(ppm_in)) {
             newPpm = ppm_in;
         }
     } else {
         newPpm = 0;
     }
     ppmChanged = true;
     SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR PPM: %d", newPpm);

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
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Changing gain mode to %s", automatic?"Automatic":"Manual");
    newAgcMode = automatic;
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
    if (name == "IF") {
        newIFGain = value;
        IFGainChanged = true;
    }

    if (name == "TUNER") {
        newTunerGain = value;
        tunerGainChanged = true;
    }
}

double SoapyRTLSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "IF") {
        if (IFGainChanged) {
            return newIFGain;
        }
        return IFGain;
    }

    if (name == "TUNER") {
        if (tunerGainChanged) {
            return newTunerGain;
        }
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
        newCenterFrequency = (uint32_t)frequency;
        centerFrequencyChanged = true;
    }

    if (name == "CORR") {
        newPpm = (int)frequency;
        ppmChanged = true;
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

    if (name == "CORR")
    {
        if (ppmChanged) {
            return (double)newPpm;
        }
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
