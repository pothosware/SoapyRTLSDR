/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapyRTLSDR.hpp"

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

   bufferSize = bufferLength * numBuffers;
   iq_input.resize(bufferSize);

   return (SoapySDR::Stream *)this;
}

void SoapyRTLSDR::closeStream(SoapySDR::Stream *stream)
{
    rtlsdr_close(dev);
}

size_t SoapyRTLSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferSize/2;
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

    if (IFGainChanged) {
        IFGain = newIFGain;
        IFGainChanged = false;
        rtlsdr_set_tuner_if_gain(dev, 0, (int)IFGain*10.0);
    }

    if (tunerGainChanged) {
        tunerGain = newTunerGain;
        tunerGainChanged = false;
        rtlsdr_set_tuner_gain(dev, (int)tunerGain*10.0);
    }

    if (resetBuffer) {
        rtlsdr_reset_buffer(dev);
    }


    // Prevent stalling if we've already buffered enough data..
    if ((iq_buffer.size()/2) < numElems)
    {
        int n_read = 0;

//        for (int i = 0; i < numBuffers; i++) {
//            //receive into temporary buffer
//            int buf_read;
//            rtlsdr_read_sync(dev, &iq_input[n_read], bufferLength, &buf_read);
//            n_read += buf_read;
//        }

        rtlsdr_read_sync(dev, &iq_input[0], bufferLength*numBuffers, &n_read);

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
