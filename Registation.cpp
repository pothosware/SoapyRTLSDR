/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapyRTLSDR.hpp"
#include <SoapySDR/Registry.hpp>

static std::vector<SoapySDR::Kwargs> findRTLSDR(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    //TODO filtering
    //when the user passes input args like a serial number
    //or identification number. Use this information
    //to filter the results when the specified keys do not match

    char manufact[256], product[256], serial[256];

    int rtl_count = rtlsdr_get_device_count();

    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Devices: %d", rtl_count);

    for (int i = 0; i < rtl_count; i++) {
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

        devInfo["device_index"] = std::to_string(i);
        devInfo["label"] = rtlsdr_get_device_name(i);
        devInfo["available"] = deviceAvailable?"Yes":"No";
        devInfo["product"] = deviceProduct;
        devInfo["serial"] = deviceSerial;
        devInfo["manufacturer"] = deviceManufacturer;
        devInfo["tuner"] = deviceTuner;
        results.push_back(devInfo);
    }

    return results;
}

static SoapySDR::Device *makeRTLSDR(const SoapySDR::Kwargs &args)
{
    return new SoapyRTLSDR(args);
}

static SoapySDR::Registry registerRTLSDR("rtlsdr", &findRTLSDR, &makeRTLSDR, SOAPY_SDR_ABI_VERSION);
