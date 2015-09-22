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

   if (!_lut_32f.size()) {
       SoapySDR_logf(SOAPY_SDR_DEBUG, "Generating RTL-SDR lookup tables");
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

           std::complex<int16_t> v16i, vs16i;

           v16i.real(int16_t((float(SHRT_MAX) * ((float(i & 0xff) - 127.4f) * (1.0f/128.0f)))));
           v16i.imag(int16_t((float(SHRT_MAX) * ((float(i >> 8) - 127.4f) * (1.0f/128.0f)))));
           _lut_16i.push_back(v16i);

           vs16i.real(vs16i.imag());
           vs16i.imag(vs16i.real());
           _lut_swap_16i.push_back(vs16i);

#else // BIG_ENDIAN
#error  TODO
           //        tmp_swap.imag = tmp.real = (float(i >> 8) - 127.4f) * (1.0f/128.0f);
           //        tmp_swap.real = tmp.imag = (float(i & 0xff) - 127.4f) * (1.0f/128.0f);
           //        _lut.push_back(tmp);
           //        _lut_swap.push_back(tmp_swap);
#endif
       }
   }

   //check the format
   if (format == "CF32")
   {
       SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
       rxFormat = RTL_RX_FORMAT_FLOAT32;
   }
   else if (format == "CS16")
   {
       SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
       rxFormat = RTL_RX_FORMAT_INT16;
   }
   else
   {
       throw std::runtime_error("setupStream invalid format '" + format + "' -- Only CS16 and CF32 are supported by SoapyRTLSDR module.");
   }

   bufferSize = bufferLength * numBuffers;
   iq_input.resize(bufferSize);

   return (SoapySDR::Stream *)this;
}

void SoapyRTLSDR::closeStream(SoapySDR::Stream *stream)
{

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
     resetBuffer = true;
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

    if (resetBuffer) {
        resetBuffer = false;
        iq_buffer.erase(iq_buffer.begin(), iq_buffer.end());
        rtlsdr_reset_buffer(dev);
    }

    // Prevent stalling if we've already buffered enough data..
    if ((iq_buffer.size()/2) < numElems)
    {
        int n_read = 0;

        rtlsdr_read_sync(dev, &iq_input[0], bufferLength*numBuffers, &n_read);

        //was numElems < than the hardware transfer size?
        //may have to keep part of that temporary buffer
        //around for the next call into readStream...
        if (n_read) {
            iq_buffer.insert(iq_buffer.end(),iq_input.begin(),iq_input.begin()+n_read);
        }
    }

    int numElemsBuffered = iq_buffer.size()/2;
    int returnedElems = (numElems>numElemsBuffered)?numElemsBuffered:numElems;

    if (!returnedElems) {
        return 0;
    }

    uint16_t idx;

    //convert into user's buff0
    if (rxFormat == RTL_RX_FORMAT_FLOAT32)
    {
        float *ftarget = (float *)buff0;
        if (iqSwap) {
            for (int i = 0; i < returnedElems; i++)
            {
                idx = *((uint16_t*)&iq_buffer[2*i]);
                ftarget[i*2] = _lut_swap_32f[idx].real();
                ftarget[i*2+1] = _lut_swap_32f[idx].imag();
            }
        } else {
            for (int i = 0; i < returnedElems; i++)
            {
                idx = *((uint16_t*)&iq_buffer[2*i]);
                ftarget[i*2] = _lut_32f[idx].real();
                ftarget[i*2+1] = _lut_32f[idx].imag();
            }
        }
    }
    else if (rxFormat == RTL_RX_FORMAT_INT16)
    {
        int16_t *itarget = (int16_t *)buff0;
        if (iqSwap) {
            for (int i = 0; i < returnedElems; i++)
            {
                idx = *((uint16_t*)&iq_buffer[2*i]);
                itarget[i*2] = _lut_swap_16i[idx].real();
                itarget[i*2+1] = _lut_swap_16i[idx].imag();
            }
        } else {
            for (int i = 0; i < returnedElems; i++)
            {
                idx = *((uint16_t*)&iq_buffer[2*i]);
                itarget[i*2] = _lut_16i[idx].real();
                itarget[i*2+1] = _lut_16i[idx].imag();
            }
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
