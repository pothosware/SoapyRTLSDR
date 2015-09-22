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

static std::vector<SoapySDR::Kwargs> findRTLSDR(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    char manufact[256], product[256], serial[256];

    int this_count = rtlsdr_get_device_count();

    if (!SoapyRTLSDR::rtl_devices.size() || SoapyRTLSDR::rtl_count != this_count) {
        SoapyRTLSDR::rtl_count = this_count;

        if (SoapyRTLSDR::rtl_devices.size()) {
            SoapyRTLSDR::rtl_devices.erase(SoapyRTLSDR::rtl_devices.begin(),SoapyRTLSDR::rtl_devices.end());
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Devices: %d", SoapyRTLSDR::rtl_count);

        for (int i = 0; i < SoapyRTLSDR::rtl_count; i++) {
            SoapySDR::Kwargs devInfo;

            std::string deviceName(rtlsdr_get_device_name(i));
            std::string deviceManufacturer;
            std::string deviceProduct;
            std::string deviceTuner;
            std::string deviceSerial;

            bool deviceAvailable = false;
            SoapySDR_logf(SOAPY_SDR_DEBUG, "Device #%d: %s", i, deviceName.c_str());
            if (rtlsdr_get_device_usb_strings(i, manufact, product, serial) == 0) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "\tManufacturer: %s, Product Name: %s, Serial: %s", manufact, product, serial);

                deviceSerial = serial;
                deviceAvailable = true;
                deviceProduct = product;
                deviceManufacturer = manufact;

                rtlsdr_dev_t *devTest;
                rtlsdr_open(&devTest, i);

                if (!SoapyRTLSDR::gainMax) {
                    int num_gains = rtlsdr_get_tuner_gains(devTest, NULL);
                    int *gains = (int *)malloc(sizeof(int) * num_gains);

                    num_gains = rtlsdr_get_tuner_gains(devTest, gains);

                    int rangeMin = gains[0], rangeMax = gains[0];

                    for (int g = 0; g < num_gains; g++) {
                        if (gains[g] < rangeMin) {
                            rangeMin = gains[g];
                        }
                        if (gains[g] > rangeMax) {
                            rangeMax = gains[g];
                        }
                    }
                    free(gains);

                    SoapyRTLSDR::gainMin = (double)rangeMin/10.0;
                    SoapyRTLSDR::gainMax = (double)rangeMax/10.0;
                }

                switch (rtlsdr_get_tuner_type(devTest)) {
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
                }

                SoapySDR_logf(SOAPY_SDR_DEBUG, "\t Tuner type: %s", deviceTuner.c_str());

                rtlsdr_close(devTest);
            } else {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "\tUnable to access device #%d (in use?)", i);
            }

            std::string deviceLabel = std::string(rtlsdr_get_device_name(i)) + " :: " + deviceSerial;


            devInfo["rtl"] = std::to_string(i);
            devInfo["label"] = deviceLabel;
            devInfo["available"] = deviceAvailable?"Yes":"No";
            devInfo["product"] = deviceProduct;
            devInfo["serial"] = deviceSerial;
            devInfo["manufacturer"] = deviceManufacturer;
            devInfo["tuner"] = deviceTuner;
            SoapyRTLSDR::rtl_devices.push_back(devInfo);
        }
    }

    //filtering
    for (int i = 0; i < SoapyRTLSDR::rtl_count; i++) {
        SoapySDR::Kwargs devInfo = SoapyRTLSDR::rtl_devices[i];
        if (args.count("rtl") != 0) {
            if (args.at("rtl") != devInfo.at("rtl")) {
                continue;
            }
            SoapySDR_logf(SOAPY_SDR_DEBUG, "Found device by index %s", devInfo.at("rtl").c_str());
        } else if (args.count("serial") != 0) {
            if (devInfo.at("serial") != args.at("serial")) {
                continue;
            }
            SoapySDR_logf(SOAPY_SDR_DEBUG, "Found device by serial %s", args.at("serial").c_str());
        } else if (args.count("label") != 0) {
            if (devInfo.at("label") != args.at("label")) {
                continue;
            }
            SoapySDR_logf(SOAPY_SDR_DEBUG, "Found device by label %s", args.at("label").c_str());
        }
        results.push_back(SoapyRTLSDR::rtl_devices[i]);
    }
    return results;
}

static SoapySDR::Device *makeRTLSDR(const SoapySDR::Kwargs &args)
{
    return new SoapyRTLSDR(args);
}

static SoapySDR::Registry registerRTLSDR("rtlsdr", &findRTLSDR, &makeRTLSDR, SOAPY_SDR_ABI_VERSION);
