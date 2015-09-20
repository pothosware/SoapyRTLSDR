/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapyRTLSDR.hpp"

#define DEFAULT_BUFFER_SIZE (16384*6)


SoapySDR::Stream *SoapyRTLSDR::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
   if (direction != SOAPY_SDR_RX) {
       throw std::runtime_error("SDRPlay is RX only, use SOAPY_SDR_RX");
   }

   //check the channel configuration
   if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
   {
       throw std::runtime_error("setupStream invalid channel selection");
   }

   //check the format
   if (format == "CF32")
   {
       SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
       rxFormat = RTL_RX_FORMAT_FLOAT32;
   }
   else if (format == "CF16")
   {
       SoapySDR_log(SOAPY_SDR_INFO, "Using format CF16.");
       rxFormat = RTL_RX_FORMAT_FLOAT32;
   }
   else if (format == "CS16")
   {
       SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
       rxFormat = RTL_RX_FORMAT_INT16;
   }
   else
   {
       throw std::runtime_error("setupStream invalid format '" + format + "' -- Only C8, CS16, CF16 and CF32 are supported by SoapyRTLSDR, and CS8 is the native format.");
   }

   if (args.find("buffer_size") != args.end()) {
       int bufferSize_in = std::stoi(args.at("buffer_size"));
       if (!std::isnan(bufferSize_in) && bufferSize_in) {
           bufferSize = bufferSize_in;
       }
   } else {
       bufferSize = DEFAULT_BUFFER_SIZE;
   }
   SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using buffer size %d", bufferSize);

  if (args.find("direct_sampling_mode") != args.end()) {
       int directSamplingMode_in = std::stoi(args.at("direct_sampling_mode"));
       if (!std::isnan(directSamplingMode_in)) {
           if (directSamplingMode_in >= 0 && directSamplingMode_in <= 2) {
               newDirectSamplingMode = directSamplingMode_in;
           } else {
               throw std::runtime_error("direct_sampling_mode " + std::to_string(directSamplingMode_in) + " invalid.  0 = off, 1 = I ADC, 2 = Q ADC");
           }
       }
   } else {
       newDirectSamplingMode = 0;
   }
   directSamplingModeChanged = true;
   SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode %d", newDirectSamplingMode);


   if (args.find("iq_swap") != args.end()) {
       int iqSwap_in = std::stoi(args.at("iq_swap"));
       if (!std::isnan(iqSwap_in)) {
           iqSwap = iqSwap_in?true:false;
       }
   } else {
       iqSwap = 0;
   }
   iqSwapChanged = false;
   SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap?"Yes":"No");

   if (args.find("offset_tuning") != args.end()) {
        int offsetMode_in = std::stoi(args.at("offset_tuning"));
        if (!std::isnan(offsetMode_in)) {
            newOffsetMode = offsetMode_in?true:false;
        }
    } else {
        newOffsetMode = false;
    }
   offsetModeChanged = true;
   SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tuning mode: %s", offsetMode?"Yes":"No");

   if (args.find("ppm") != args.end()) {
       int ppm_in = std::stoi(args.at("ppm"));
       if (!std::isnan(ppm_in)) {
           newPpm = ppm_in;
       }
   } else {
       newPpm = 0;
   }
   ppmChanged = true;
   SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR PPM: %d", newPpm);


   iq_input.resize(bufferSize);

   return (SoapySDR::Stream *)this;
}

void SoapyRTLSDR::closeStream(SoapySDR::Stream *stream)
{
    rtlsdr_close(dev);
}

size_t SoapyRTLSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferSize*2;
}

int SoapyRTLSDR::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    // Open RTL-SDR device
     SoapySDR_logf(SOAPY_SDR_DEBUG, "Opening RTL-SDR device %d", deviceId);
     rtlsdr_open(&dev, deviceId);

     return 0;
}

int SoapyRTLSDR::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    return 0;
}

int SoapyRTLSDR::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    //this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    bool resetBuffer = false;

    if (sampleRateChanged) {
        sampleRate = newSampleRate;
        sampleRateChanged = false;
        resetBuffer = true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
        rtlsdr_set_sample_rate(dev, sampleRate);
    }

    if (centerFrequencyChanged) {
        centerFrequency = newCenterFrequency;
        centerFrequencyChanged = false;
        resetBuffer = true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", centerFrequency);
        rtlsdr_set_center_freq(dev, centerFrequency);
    }

    if (ppmChanged) {
        ppm = newPpm;
        ppmChanged = false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR ppm: %d", ppm);

        rtlsdr_set_freq_correction(dev, ppm);
    }

    if (agcModeChanged) {
        agcMode = newAgcMode;
        agcModeChanged = false;

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR AGC: %s", agcMode?"On":"Off");
        rtlsdr_set_agc_mode(dev, agcMode?1:0);
    }

    if (offsetModeChanged) {
        offsetMode = newOffsetMode;
        offsetModeChanged = false;

        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR Offset Tuning: %s", offsetMode?"On":"Off");
        rtlsdr_set_offset_tuning(dev, offsetMode?1:0);
    }

    if (resetBuffer) {
        rtlsdr_reset_buffer(dev);
    }


    // Prevent stalling if we've already buffered enough data..
    while ((iq_buffer.size()/2) < numElems)
    {
        int n_read;

        //receive into temporary buffer
        rtlsdr_read_sync(dev, &iq_input[0], bufferSize, &n_read);

        if (!n_read) {
            return SOAPY_SDR_UNDERFLOW;
        }
        //was numElems < than the hardware transfer size?
        //may have to keep part of that temporary buffer
        //around for the next call into readStream...
        iq_buffer.insert(iq_buffer.end(),iq_input.begin(),iq_input.begin()+n_read);
    }

    int numElemsBuffered = iq_buffer.size()/2;
    int returnedElems = (numElems>numElemsBuffered)?numElemsBuffered:numElems;

    if (!returnedElems) {
        return SOAPY_SDR_UNDERFLOW;
    }

    uint16_t idx;

    //convert into user's buff0
    if (rxFormat == RTL_RX_FORMAT_FLOAT32)
    {
        float *ftarget = (float *)buff0;
        for (int i = 0; i < returnedElems; i++)
        {
            idx = *((uint16_t*)&iq_buffer[2*i]);
            ftarget[i*2] = _lut_32f[idx].real();
            ftarget[i*2+1] = _lut_32f[idx].imag();
        }
    }
    else
    {
        throw std::runtime_error("Selected format is not yet implmented..");
    }

    iq_buffer.erase(iq_buffer.begin(),iq_buffer.begin()+(returnedElems*2));

    //return number of elements written to buff0
    return returnedElems;
}
