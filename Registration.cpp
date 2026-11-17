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
#include <SoapySDR/Registry.hpp>
#include <mutex>
#include <map>

//lookup the tuner by opening the device, it's used in the discovery arguments
//the tuner is cached because the device cannot be opened twice in the same process,
//and we require that findRTLSDR() yield the same results for SoapySDR device cache.
//if another process attempts to find an open rtlsdr, it will be marked unavailable
// USB indexes identify receivers on every platform, including devices with
// duplicate or empty serials. Only tuner metadata probing is browser-specific:
// native libusb can safely open devices during discovery, while WebUSB cannot.
#ifndef __EMSCRIPTEN__
static std::string get_tuner(const size_t deviceIndex)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    static std::map<size_t, std::string> cache;
    auto it = cache.find(deviceIndex);
    if (it != cache.end()) return it->second;

    rtlsdr_dev_t *devTest;
    if (rtlsdr_open(&devTest, deviceIndex) != 0) return "unavailable";
    const auto tuner = SoapyRTLSDR::rtlTunerToString(rtlsdr_get_tuner_type(devTest));
    rtlsdr_close(devTest);
    cache[deviceIndex] = tuner;
    return tuner;
}
#endif

static std::vector<SoapySDR::Kwargs> findRTLSDR(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    char manufact[256] = {}, product[256] = {}, serial[256] = {};

    const size_t this_count = rtlsdr_get_device_count();

    for (size_t i = 0; i < this_count; i++)
    {
#ifdef __EMSCRIPTEN__
        // Do not open every receiver merely to populate discovery metadata.
        // WebUSB serializes asynchronous access to each authorized USBDevice,
        // so probing one active or unresponsive receiver can block the others.
        // Keep explicit legacy serial filtering, but use the cross-platform
        // USB index as the normal device identity.
        if (args.count("serial") != 0 &&
            rtlsdr_get_device_usb_strings(i, manufact, product, serial) != 0)
        {
            SoapySDR_logf(SOAPY_SDR_ERROR, "rtlsdr_get_device_usb_strings(%zu) failed", i);
            continue;
        }
#else
        if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) != 0)
        {
            SoapySDR_logf(SOAPY_SDR_ERROR, "rtlsdr_get_device_usb_strings(%zu) failed", i);
            continue;
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "\tManufacturer: %s, Product Name: %s, Serial: %s", manufact, product, serial);
#endif

        const std::string index = std::to_string(i);

        // Index filtering works on every platform; serial remains a
        // compatibility path for existing device strings.
        if (args.count("index") != 0 and args.at("index") != index) continue;
        if (args.count("serial") != 0 and args.at("serial") != serial) continue;

        SoapySDR::Kwargs devInfo;
#ifdef __EMSCRIPTEN__
        devInfo["label"] = std::string(rtlsdr_get_device_name(i)) + " [USB index " + index + "]";
#else
        devInfo["label"] = std::string(rtlsdr_get_device_name(i)) + " :: " + serial;
        devInfo["product"] = product;
        devInfo["serial"] = serial;
        devInfo["manufacturer"] = manufact;
        devInfo["tuner"] = get_tuner(i);
#endif
        devInfo["index"] = index;

        results.push_back(devInfo);
    }

    return results;
}

static SoapySDR::Device *makeRTLSDR(const SoapySDR::Kwargs &args)
{
    return new SoapyRTLSDR(args);
}

static SoapySDR::Registry registerRTLSDR("rtlsdr", &findRTLSDR, &makeRTLSDR, SOAPY_SDR_ABI_VERSION);
