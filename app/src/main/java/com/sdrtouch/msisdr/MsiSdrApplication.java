/*
 * sdr_msi_driver is a library that uses libusb and libmsisdr to
 * turn your MSi2500/MSi001 based device into a SDR receiver.
 * It implements the rtl_tcp API protocol for native Android usage.
 * Copyright (C) 2022 by Signalware Ltd <driver@sdrtouch.com>
 *
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.sdrtouch.msisdr;

import static com.sdrtouch.msisdr.SdrDeviceProviderRegistry.SDR_DEVICE_PROVIDERS;

import android.app.Application;

import com.sdrtouch.core.devices.SdrDeviceProvider;
import com.sdrtouch.tools.StrRes;

public class MsiSdrApplication extends Application {
    public final static boolean IS_PLATFORM_SUPPORTED;

    static {
        boolean isPlatformSupported = false;
        try {
            isPlatformSupported = loadNativeLibraries();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        IS_PLATFORM_SUPPORTED = isPlatformSupported;
    }

    private static boolean loadNativeLibraries() {
        for (SdrDeviceProvider sdrDeviceProvider : SDR_DEVICE_PROVIDERS) {
            if (!sdrDeviceProvider.loadNativeLibraries()) {
                return false;
            }
        }
        return true;
    }

    public void onCreate() {
        super.onCreate();

        StrRes.res = getResources();
    }
}
