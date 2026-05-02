package com.sdrtouch.rtlsdr;

import com.sdrtouch.core.devices.SdrDeviceProvider;
import com.sdrtouch.msisdr.driver.MsiSdrDeviceProvider;

public class SdrDeviceProviderRegistry {
    final static SdrDeviceProvider[] SDR_DEVICE_PROVIDERS = new SdrDeviceProvider[] {
            new MsiSdrDeviceProvider(),
    };
}
