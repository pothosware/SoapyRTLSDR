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
#include <algorithm>
#include <climits> //SHRT_MAX

SoapySDR::Stream *SoapyRTLSDR::setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args)
{
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("RTL-SDR is RX only, use SOAPY_SDR_RX");
    }

    //check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }

    if (!_lut_32f.size())
    {
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Generating RTL-SDR lookup tables");
        // create lookup tables
        for (unsigned int i = 0; i <= 0xffff; i++)
        {
# if (__BYTE_ORDER == __LITTLE_ENDIAN)
            std::complex<float> v32f, vs32f;

            v32f.real((float(i & 0xff) - 127.4f) * (1.0f / 128.0f));
            v32f.imag((float(i >> 8) - 127.4f) * (1.0f / 128.0f));
            _lut_32f.push_back(v32f);

            vs32f.real(v32f.imag());
            vs32f.imag(v32f.real());
            _lut_swap_32f.push_back(vs32f);

            std::complex<int16_t> v16i, vs16i;

            v16i.real(int16_t((float(SHRT_MAX) * ((float(i & 0xff) - 127.4f) * (1.0f / 128.0f)))));
            v16i.imag(int16_t((float(SHRT_MAX) * ((float(i >> 8) - 127.4f) * (1.0f / 128.0f)))));
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
        throw std::runtime_error(
                "setupStream invalid format '" + format
                        + "' -- Only CS16 and CF32 are supported by SoapyRTLSDR module.");
    }

    bufferSize = bufferLength * numBuffers;
    iq_buffer.resize(bufferSize);

    return (SoapySDR::Stream *) this;
}

void SoapyRTLSDR::closeStream(SoapySDR::Stream *stream)
{

}

size_t SoapyRTLSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferSize / 2;
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

int SoapyRTLSDR::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
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

    if (resetBuffer)
    {
        resetBuffer = false;
        bufferedElems = 0;
        rtlsdr_reset_buffer(dev);
    }

    int n_read = 0;

    //are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        //receive into temp buffer
        rtlsdr_read_sync(dev, &iq_buffer[0], bufferLength * numBuffers, &n_read);
        bufferedElems = n_read / 2;
        bufferedElemOffset = 0;
    }

    size_t returnedElems = std::min((int) bufferedElems, (int) numElems);

    int buffer_ofs = (bufferedElemOffset * 2);

    //convert into user's buff0
    if (rxFormat == RTL_RX_FORMAT_FLOAT32)
    {
        float *ftarget = (float *) buff0;
        std::complex<float> tmp;
        if (iqSwap)
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_swap_32f[*((uint16_t*) &iq_buffer[buffer_ofs + 2 * i])];
                ftarget[i * 2] = tmp.real();
                ftarget[i * 2 + 1] = tmp.imag();
            }
        }
        else
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_32f[*((uint16_t*) &iq_buffer[buffer_ofs + 2 * i])];
                ftarget[i * 2] = tmp.real();
                ftarget[i * 2 + 1] = tmp.imag();
            }
        }
    }
    else if (rxFormat == RTL_RX_FORMAT_INT16)
    {
        int16_t *itarget = (int16_t *) buff0;
        std::complex<int16_t> tmp;
        if (iqSwap)
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_swap_16i[*((uint16_t*) &iq_buffer[buffer_ofs + 2 * i])];
                itarget[i * 2] = tmp.real();
                itarget[i * 2 + 1] = tmp.imag();
            }
        }
        else
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_16i[*((uint16_t*) &iq_buffer[buffer_ofs + 2 * i])];
                itarget[i * 2] = tmp.real();
                itarget[i * 2 + 1] = tmp.imag();
            }
        }
    }

    //bump variables for next call into readStream
    bufferedElems -= returnedElems;
    bufferedElemOffset += returnedElems;

    //return number of elements written to buff0
    return returnedElems;
}
