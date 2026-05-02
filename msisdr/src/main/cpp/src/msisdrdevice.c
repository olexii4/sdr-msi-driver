/*
 * rtl_tcp_andro is a library that uses libusb and librtlsdr to
 * turn your Realtek RTL2832 based DVB dongle into a SDR receiver.
 * It independently implements the rtl-tcp API protocol for native Android usage.
 * Copyright (C) 2022 by Signalware Ltd <driver@sdrtouch.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "msisdr.h"
#include "sdrtcp.h"
#include "SdrException.h"
#include "tcp_commands.h"

#define RUN_OR(command, exit_command) { \
    int cmd_result = command; \
    if (cmd_result != 0) { \
        throwExceptionWithInt(env, "com/sdrtouch/core/exceptions/SdrException", cmd_result); \
        exit_command; \
    }; \
}

#define RUN_OR_GOTO(command, label) RUN_OR(command, goto label);

typedef struct msisdr_android {
    sdrtcp_t tcpserv;
    msisdr_dev_t *miri_dev;
    volatile int enable_16bit;
    volatile int dc_correction_enabled;
    float dc_avg_i;
    float dc_avg_q;
    unsigned char *conv_buf;
    uint32_t conv_buf_size;
} msisdr_android_t;

#define WITH_DEV(x) msisdr_android_t* x = (msisdr_android_t*) pointer

void initialize(JNIEnv *env) {
    LOGI_NATIVE("Initializing MSi.SDR");
}

static int set_gain_by_index(msisdr_dev_t *_dev, unsigned int index) {
    int res = 0;
    int *gains;
    int count = msisdr_get_tuner_gains(_dev, NULL);

    if (count > 0 && (unsigned int) count > index) {
        gains = malloc(sizeof(int) * count);
        msisdr_get_tuner_gains(_dev, gains);
        res = msisdr_set_tuner_gain(_dev, gains[index]);
        free(gains);
    }

    return res;
}

static int set_gain_by_perc(msisdr_dev_t *_dev, unsigned int percent) {
    int res = 0;
    int *gains;
    int count = msisdr_get_tuner_gains(_dev, NULL);
    unsigned int index = (percent * count) / 100;
    if (index >= (unsigned int) count) index = (unsigned int) (count - 1);

    gains = malloc(sizeof(int) * count);
    msisdr_get_tuner_gains(_dev, gains);
    res = msisdr_set_tuner_gain(_dev, gains[index]);
    free(gains);

    return res;
}

static jint SUPPORTED_COMMANDS[] = {
        TCP_SET_FREQ,
        TCP_SET_SAMPLE_RATE,
        TCP_SET_GAIN_MODE,
        TCP_SET_GAIN,
        TCP_SET_FREQ_CORRECTION,
        TCP_SET_OFFSET_TUNING,
        TCP_SET_TUNER_GAIN_BY_ID,
        TCP_ANDROID_EXIT,
        TCP_ANDROID_GAIN_BY_PERCENTAGE,
        TCP_ANDROID_ENABLE_16_BIT_SIGNED,
        TCP_ANDROID_ENABLE_DC_CORRECTION,
};

void tcpCommandCallback(sdrtcp_t *tcpserv, void *pointer, sdr_tcp_command_t *cmd) {
    WITH_DEV(dev);
    if (dev->miri_dev == NULL) return;

    switch (cmd->command) {
        case TCP_SET_FREQ:
            msisdr_set_center_freq(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_SAMPLE_RATE:
            LOGI("set sample rate %d", cmd->parameter);
            msisdr_set_sample_rate(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_GAIN_MODE:
            LOGI("set gain mode %d", cmd->parameter);
            msisdr_set_tuner_gain_mode(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_GAIN:
            LOGI("set gain %d", cmd->parameter);
            msisdr_set_tuner_gain(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_FREQ_CORRECTION:
            LOGI("set freq correction %d", cmd->parameter);
            msisdr_set_freq_correction(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_OFFSET_TUNING:
            LOGI("set offset tuning %d", cmd->parameter);
            msisdr_set_offset_tuning(dev->miri_dev, cmd->parameter);
            break;
        case TCP_SET_TUNER_GAIN_BY_ID:
            LOGI("set tuner gain by index %d", cmd->parameter);
            set_gain_by_index(dev->miri_dev, cmd->parameter);
            break;
        case TCP_ANDROID_EXIT:
            LOGI("tcpCommandCallback: client requested to close msi_sdr");
            sdrtcp_stop_serving_client(tcpserv);
            break;
        case TCP_ANDROID_GAIN_BY_PERCENTAGE:
            set_gain_by_perc(dev->miri_dev, cmd->parameter);
            break;
        case TCP_ANDROID_ENABLE_16_BIT_SIGNED:
            LOGI("set 16-bit signed mode %d", cmd->parameter);
            dev->enable_16bit = cmd->parameter ? 1 : 0;
            break;
        case TCP_ANDROID_ENABLE_DC_CORRECTION:
            LOGI("set DC correction %d", cmd->parameter);
            dev->dc_correction_enabled = cmd->parameter ? 1 : 0;
            if (!dev->dc_correction_enabled) {
                dev->dc_avg_i = 0.0f;
                dev->dc_avg_q = 0.0f;
            }
            break;
        default:
            break;
    }
}

void tcpClosedCallback(sdrtcp_t *tcpserv, void *pointer) {
    WITH_DEV(dev);
    if (dev->miri_dev == NULL) return;
    msisdr_cancel_async(dev->miri_dev);
}

void msisdr_callback(unsigned char *buf, uint32_t len, void *pointer) {
    WITH_DEV(dev);
    if (dev->miri_dev == NULL) return;

    int16_t *samples = (int16_t *) buf;
    uint32_t n_samples = len / sizeof(int16_t);

    if (dev->dc_correction_enabled && n_samples >= 2) {
        for (uint32_t i = 0; i < n_samples; i += 2) {
            float si = (float) samples[i];
            float sq = (float) samples[i + 1];
            dev->dc_avg_i = 0.998f * dev->dc_avg_i + 0.002f * si;
            dev->dc_avg_q = 0.998f * dev->dc_avg_q + 0.002f * sq;
            int32_t ci = (int32_t)(si - dev->dc_avg_i);
            int32_t cq = (int32_t)(sq - dev->dc_avg_q);
            if (ci > 32767) ci = 32767; else if (ci < -32768) ci = -32768;
            if (cq > 32767) cq = 32767; else if (cq < -32768) cq = -32768;
            samples[i] = (int16_t) ci;
            samples[i + 1] = (int16_t) cq;
        }
    }

    if (dev->enable_16bit) {
        sdrtcp_feed(&dev->tcpserv, buf, len / 2);
    } else {
        unsigned char *out = dev->conv_buf;
        if (!out || n_samples > dev->conv_buf_size) return;
        for (uint32_t i = 0; i < n_samples; i++) {
            out[i] = (unsigned char) ((samples[i] >> 8) + 128);
        }
        sdrtcp_feed(&dev->tcpserv, out, n_samples / 2);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_sdrtouch_msisdr_driver_MsiSdrDevice_openAsync(
        JNIEnv *env, jobject instance, jlong pointer, jint fd, jint gain, jlong samplingrate,
        jlong frequency, jint port, jint ppm, jstring address_, jstring devicePath_) {
    WITH_DEV(dev);
    const char *devicePath = (*env)->GetStringUTFChars(env, devicePath_, 0);
    const char *address = (*env)->GetStringUTFChars(env, address_, 0);

    EXCEPT_SAFE_NUM(jclass clazz = (*env)->GetObjectClass(env, instance));
    EXCEPT_SAFE_NUM(jmethodID announceOnOpen = (*env)->GetMethodID(env, clazz, "announceOnOpen", "()V"));

    msisdr_dev_t *device = NULL;
    RUN_OR_GOTO(msisdr_open_fd(&device, fd, devicePath), rel_jni);

    if (ppm != 0) {
        if (msisdr_set_freq_correction(device, ppm) < 0) {
            LOGI("WARNING: Failed to set ppm to %d", ppm);
        }
    }

    int result = 0;
    if (samplingrate < 0 || (result = msisdr_set_sample_rate(device, (uint32_t) samplingrate)) < 0) {
        LOGI("ERROR: Failed to set sample rate to %lld", samplingrate);
        if (result == -1 || result == -7) {
            RUN_OR(EXIT_NOT_ENOUGH_POWER, goto err);
        } else {
            RUN_OR(EXIT_WRONG_ARGS, goto err);
        }
    } else {
        LOGI("Set sampling rate to %lld", samplingrate);
    }

    if (frequency < 0 || msisdr_set_center_freq(device, (uint32_t) frequency) < 0) {
        LOGI("ERROR: Failed to set frequency to %lld", frequency);
        RUN_OR(EXIT_WRONG_ARGS, goto err);
    }

    if (0 == gain) {
        if (msisdr_set_tuner_gain_mode(device, 0) < 0)
            LOGI("WARNING: Failed to enable automatic gain");
    } else {
        if (msisdr_set_tuner_gain_mode(device, 1) < 0)
            LOGI("WARNING: Failed to enable manual gain");

        if (msisdr_set_tuner_gain(device, gain) < 0)
            LOGI("WARNING: Failed to set tuner gain");
        else
            LOGI("Tuner gain set to %f dB", gain / 10.0);
    }

    if (msisdr_reset_buffer(device) < 0)
        LOGI("WARNING: Failed to reset buffers");

    uint32_t gains_count = (uint32_t) msisdr_get_tuner_gains(device, NULL);
    if (!sdrtcp_open_socket(&dev->tcpserv, address, port, "MIRI", 0, gains_count)) {
        RUN_OR(EXIT_WRONG_ARGS, goto err);
    }

    dev->conv_buf_size = 32768;
    dev->conv_buf = malloc(dev->conv_buf_size);

    dev->miri_dev = device;
    sdrtcp_serve_client_async(&dev->tcpserv, (void *) dev, tcpCommandCallback, tcpClosedCallback);

    int successful = 1;
    EXCEPT_DO((*env)->CallVoidMethod(env, instance, announceOnOpen), successful = 0);
    if (msisdr_read_async(device, msisdr_callback, (void *) dev, 0, 0)) {
        LOGI("msisdr_read_async failed");
        successful = 0;
    }
    LOGI("msisdr_read_async finished");

    if (dev->conv_buf) {
        free(dev->conv_buf);
        dev->conv_buf = NULL;
    }

    dev->miri_dev = NULL;
    msisdr_close(device);
    sdrtcp_stop_serving_client(&dev->tcpserv);

    (*env)->ReleaseStringUTFChars(env, address_, address);
    (*env)->ReleaseStringUTFChars(env, devicePath_, devicePath);

    return successful ? ((jboolean) JNI_TRUE) : ((jboolean) JNI_FALSE);

err:
    msisdr_close(device);

rel_jni:
    (*env)->ReleaseStringUTFChars(env, address_, address);
    (*env)->ReleaseStringUTFChars(env, devicePath_, devicePath);

    return (jboolean) JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_sdrtouch_msisdr_driver_MsiSdrDevice_initialize(JNIEnv *env, jobject instance) {
    msisdr_android_t *ptr = malloc(sizeof(msisdr_android_t));
    sdrtcp_init(&ptr->tcpserv);
    ptr->miri_dev = NULL;
    ptr->enable_16bit = 0;
    ptr->dc_correction_enabled = 1;
    ptr->dc_avg_i = 0.0f;
    ptr->dc_avg_q = 0.0f;
    ptr->conv_buf = NULL;
    ptr->conv_buf_size = 0;
    return (jlong) ptr;
}

JNIEXPORT void JNICALL
Java_com_sdrtouch_msisdr_driver_MsiSdrDevice_deInit(JNIEnv *env, jobject instance, jlong pointer) {
    WITH_DEV(dev);
    sdrtcp_free(&dev->tcpserv);
    if (dev->miri_dev != NULL) {
        msisdr_close(dev->miri_dev);
        dev->miri_dev = NULL;
    }
    if (dev->conv_buf) {
        free(dev->conv_buf);
        dev->conv_buf = NULL;
    }
    free((void *) dev);
}

JNIEXPORT void JNICALL
Java_com_sdrtouch_msisdr_driver_MsiSdrDevice_close__J(JNIEnv *env, jobject instance,
                                                      jlong pointer) {
    WITH_DEV(dev);
    sdrtcp_stop_serving_client(&dev->tcpserv);
}

JNIEXPORT jobjectArray JNICALL
Java_com_sdrtouch_msisdr_driver_MsiSdrDevice_getSupportedCommands(JNIEnv *env, jobject instance) {
    jint *commands = (jint *) SUPPORTED_COMMANDS;
    int n_commands = sizeof(SUPPORTED_COMMANDS) / sizeof(SUPPORTED_COMMANDS[0]);

    jintArray result;
    result = (*env)->NewIntArray(env, n_commands);
    if (result == NULL) return NULL;

    (*env)->SetIntArrayRegion(env, result, 0, n_commands, commands);
    return result;
}
