package com.sdrtouch.msisdr.driver;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;

import com.sdrtouch.core.SdrTcpArguments;
import com.sdrtouch.core.devices.SdrDevice;
import com.sdrtouch.core.exceptions.SdrException;
import com.sdrtouch.tools.Log;
import com.sdrtouch.tools.UsbPermissionObtainer;

import java.util.concurrent.ExecutionException;

public class MsiSdrDevice extends SdrDevice {
    private final UsbDevice usbDevice;
    private final long nativeHandler;

    public MsiSdrDevice(Context context, UsbDevice usbDevice) {
        super(context);
        this.usbDevice = usbDevice;
        this.nativeHandler = initialize();
    }

    @Override
    public void openAsync(final SdrTcpArguments sdrTcpArguments) {
        new Thread() {
            @Override
            public void run() {
                try {
                    int fd = openSessionAndGetFd();
                    String path = usbDevice.getDeviceName();
                    if (!openAsync(nativeHandler, fd, sdrTcpArguments.getGain(),
                            sdrTcpArguments.getSamplerateHz(), sdrTcpArguments.getFrequencyHz(),
                            sdrTcpArguments.getPort(), sdrTcpArguments.getPpm(),
                            sdrTcpArguments.getAddress(), path)) {
                        announceOnClosed(new SdrException(SdrException.EXIT_UNKNOWN));
                    } else {
                        announceOnClosed(null);
                    }
                } catch (Throwable e) {
                    announceOnClosed(e);
                }
            }
        }.start();
    }

    @Override
    public void close() {
        close(nativeHandler);
    }

    @Override
    public String getName() {
        return "msi-sdr " + usbDevice.getDeviceName();
    }

    private int openSessionAndGetFd() throws ExecutionException, InterruptedException {
        UsbDeviceConnection deviceConnection = UsbPermissionObtainer.obtainFdFor(context, usbDevice).get();
        if (deviceConnection == null) throw new RuntimeException("Could not get a connection");
        int fd = deviceConnection.getFileDescriptor();
        Log.appendLine("Opening fd " + fd);
        return fd;
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        deInit(nativeHandler);
    }

    @Override
    public native int[] getSupportedCommands();

    private native long initialize();
    private native void close(long pointer);
    private native void deInit(long pointer);
    private native boolean openAsync(long pointer, int fd, int gain, long samplingrate,
                                     long frequency, int port, int ppm,
                                     String address, String devicePath) throws Exception;
}
