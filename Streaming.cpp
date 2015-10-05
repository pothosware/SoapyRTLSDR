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
#include <SoapySDR/Logger.hpp>
#include <algorithm> //min
#include <climits> //SHRT_MAX
#include <cstring> // memcpy

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
    else if (format == "CS8") {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS8.");
        rxFormat = RTL_RX_FORMAT_INT8;
    }
    else
    {
        throw std::runtime_error(
                "setupStream invalid format '" + format
                        + "' -- Only CS8, CS16 and CF32 are supported by SoapyRTLSDR module.");
    }

    if (rxFormat != RTL_RX_FORMAT_INT8 && !_lut_32f.size())
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

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR opening device %d", deviceId);
    rtlsdr_open(&dev, deviceId);

    if (args.count("buflen") != 0)
    {
        try
        {
            int bufferLength_in = std::stoi(args.at("buflen"));
            if (bufferLength_in > 0)
            {
                bufferLength = bufferLength_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using buffer length %d", bufferLength);

    if (args.count("buffers") != 0)
    {
        try
        {
            int numBuffers_in = std::stoi(args.at("buffers"));
            if (numBuffers_in > 0)
            {
                numBuffers = numBuffers_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using %d buffers", numBuffers);

    if (args.count("direct_samp") != 0)
    {
        try
        {
            directSamplingMode = std::stoi(args.at("direct_samp"));
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode %d", directSamplingMode);

    if (args.count("iq_swap") != 0)
    {
        try
        {
            iqSwap = std::stoi(args.at("iq_swap"))? true : false;
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap ? "Yes" : "No");

    if (args.count("offset_tune") != 0)
    {
        try
        {
            offsetMode = std::stoi(args.at("offset_tune"))? true : false;
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode ? "Yes" : "No");
    rtlsdr_set_offset_tuning(dev, offsetMode ? 1 : 0);

    if (args.count("corr") != 0)
    {
        try
        {
            ppm = std::stoi(args.at("corr"));
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR PPM: %d", ppm);

    for (int i = 0; i < 6; i++)
    {
        IFGain[i] = 0;
    }
    tunerGain = rtlsdr_get_tuner_gain(dev);

    //clear async fifo counts
    _buf_tail = 0;
    _buf_count = 0;
    _buf_head = 0;

    //allocate buffers
    _buffs.resize(numBuffers);
    for (auto &buff : _buffs) buff.reserve(bufferLength);
    for (auto &buff : _buffs) buff.resize(bufferLength);

    return (SoapySDR::Stream *) this;
}

void SoapyRTLSDR::closeStream(SoapySDR::Stream *stream)
{
    _buffs.clear();
    rtlsdr_close(dev);
}

size_t SoapyRTLSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferLength / BYTES_PER_SAMPLE;
}

int SoapyRTLSDR::activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems)
{
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;
    resetBuffer = true;

    //start the async thread
    if (not _rx_async_thread.joinable())
    {
        rtlsdr_reset_buffer(dev);
        _rx_async_thread = std::thread(&SoapyRTLSDR::rx_async_operation, this);
    }

    return 0;
}

static void _rx_callback(unsigned char *buf, uint32_t len, void *ctx)
{
    //printf("_rx_callback\n");
    SoapyRTLSDR *self = (SoapyRTLSDR *)ctx;
    self->rx_callback(buf, len);
}

void SoapyRTLSDR::rx_async_operation(void)
{
    //printf("rx_async_operation\n");
    rtlsdr_read_async(dev, &_rx_callback, this, numBuffers, bufferLength);
    //printf("rx_async_operation done!\n");
}

void SoapyRTLSDR::rx_callback(unsigned char *buf, uint32_t len)
{
    std::unique_lock<std::mutex> lock(_buf_mutex);

    //printf("_rx_callback %d _buf_head=%d, numBuffers=%d\n", len, _buf_head, _buf_tail);

    //overflow condition:
    //the caller is not reading fast enough: remove all enqueued samples
    //the queue is now clear for the caller to try and catch up again
    //numBuffers-1, because one buffer may be held by bufferedElems
    if (_buf_count >= (numBuffers-1))
    {
        _buf_head = (_buf_head + _buf_count) % numBuffers;
        _buf_count = 0;
        bufferedElems = 0;
        _overflowEvent = true;
    }

    //copy into the buffer queue
    auto &buff = _buffs[_buf_tail];
    buff.resize(len);
    std::memcpy(buff.data(), buf, len);

    //increment the tail pointer
    _buf_tail = (_buf_tail + 1) % numBuffers;
    _buf_count++;

    //notify readStream()
    _buf_cond.notify_one();
}

int SoapyRTLSDR::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;
    if (_rx_async_thread.joinable())
    {
        rtlsdr_cancel_async(dev);
        _rx_async_thread.join();
    }
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

    //reset is issued by various settings
    //to drain old data out of the queue
    if (resetBuffer)
    {
        //drain all buffers from the fifo
        std::unique_lock <std::mutex> lock(_buf_mutex);
        _buf_head = (_buf_head + _buf_count) % numBuffers;
        _buf_count = 0;
        resetBuffer = false;
        bufferedElems = 0;
        _overflowEvent = false;
    }

    //are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        std::unique_lock<std::mutex> lock(_buf_mutex);

        if (_overflowEvent)
        {
            _overflowEvent = false;
            SoapySDR::log(SOAPY_SDR_SSI, "O");
            return SOAPY_SDR_OVERFLOW;
        }

        while (_buf_count == 0)
        {
            _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs));
            if (_buf_count == 0) return SOAPY_SDR_TIMEOUT;
        }

        _buf_head = (_buf_head + 1) % numBuffers;
        _buf_count--;

        _currentBuff = _buffs[_buf_head].data();
        bufferedElems = _buffs[_buf_head].size() / BYTES_PER_SAMPLE;
    }

    size_t returnedElems = std::min(bufferedElems, numElems);

    //convert into user's buff0
    if (rxFormat == RTL_RX_FORMAT_FLOAT32)
    {
        float *ftarget = (float *) buff0;
        std::complex<float> tmp;
        if (iqSwap)
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_swap_32f[*((uint16_t*) &_currentBuff[2 * i])];
                ftarget[i * 2] = tmp.real();
                ftarget[i * 2 + 1] = tmp.imag();
            }
        }
        else
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_32f[*((uint16_t*) &_currentBuff[2 * i])];
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
                tmp = _lut_swap_16i[*((uint16_t*) &_currentBuff[2 * i])];
                itarget[i * 2] = tmp.real();
                itarget[i * 2 + 1] = tmp.imag();
            }
        }
        else
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                tmp = _lut_16i[*((uint16_t*) &_currentBuff[2 * i])];
                itarget[i * 2] = tmp.real();
                itarget[i * 2 + 1] = tmp.imag();
            }
        }
    }
    else if (rxFormat == RTL_RX_FORMAT_INT8)
    {
        int8_t *itarget = (int8_t *) buff0;
        if (iqSwap)
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                itarget[i * 2] = _currentBuff[i * 2 + 1]-127;
                itarget[i * 2 + 1] = _currentBuff[i * 2]-127;
            }
        }
        else
        {
            for (size_t i = 0; i < returnedElems; i++)
            {
                itarget[i * 2] = _currentBuff[i * 2]-127;
                itarget[i * 2 + 1] = _currentBuff[i * 2 + 1]-127;
            }
        }
    }

    //bump variables for next call into readStream
    bufferedElems -= returnedElems;
    _currentBuff += returnedElems*BYTES_PER_SAMPLE;

    //return number of elements written to buff0
    if (bufferedElems != 0) flags |= SOAPY_SDR_MORE_FRAGMENTS;
    return returnedElems;
}
