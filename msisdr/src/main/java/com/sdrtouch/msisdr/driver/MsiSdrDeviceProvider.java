package com.sdrtouch.msisdr.driver;

import android.content.Context;
import android.hardware.usb.UsbDevice;

import com.sdrtouch.core.devices.SdrDevice;
import com.sdrtouch.core.devices.SdrDeviceProvider;
import com.sdrtouch.tools.UsbPermissionHelper;

import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import com.sdrtouch.msisdr.R;

public class MsiSdrDeviceProvider implements SdrDeviceProvider {
    @Override
    public List<SdrDevice> listDevices(Context ctx, boolean forceRoot) {
        Set<UsbDevice> availableUsbDevices = UsbPermissionHelper.getAvailableUsbDevices(ctx, R.xml.msi_sdr_device_filter);
        List<SdrDevice> devices = new LinkedList<>();
        for (UsbDevice usbDevice : availableUsbDevices) devices.add(new MsiSdrDevice(ctx, usbDevice));
        return devices;
    }

    @Override
    public String getName() {
        return "MsiSdr";
    }

    @Override
    public boolean loadNativeLibraries() {
        try {
            System.loadLibrary("msiSdrAndroid");
            return true;
        } catch (Throwable t) {
            t.printStackTrace();
            return false;
        }
    }
}
