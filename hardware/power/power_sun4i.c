/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "SUN4I PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define SCALINGMAXFREQ "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define SCALING_GOVERNOR "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define BOOSTPULSE_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define BOOSTPULSE_ONDEMAND "/sys/devices/system/cpu/cpufreq/ondemand/boostpulse"

#define MAX_BUF_SZ  20

static char screen_off_max_freq[MAX_BUF_SZ] = "696000";
static char scaling_max_freq[MAX_BUF_SZ] = "1008000";
static char current_governor[MAX_BUF_SZ] = { 0 };

struct sun4i_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
};

static int sysfs_read(char *path, char *s, int num_bytes)
{
    char buf[80];
    int count;
    int ret = 0;
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);

        return -1;
    }

    if ((count = read(fd, s, num_bytes - 1)) < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error reading %s: %s\n", path, buf);

        ret = -1;

    } else
        s[count] = '\0';

    close(fd);

    return ret;
}

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static int get_scaling_governor(char governor[], int size) {
    if (sysfs_read(SCALING_GOVERNOR, governor, size) == -1) {
        return -1;

    } else {
        int len = strlen(governor);

        len--;
        while (len >= 0 && (governor[len] == '\n'
			 || governor[len] == '\r'))
            governor[len--] = '\0';
    }

    return 0;
}

static void sun4i_power_init(struct power_module *module)
{
    struct sun4i_power_module *sun4i;
    char governor[80];

    if (get_scaling_governor(governor, sizeof(governor)) < 0) {
        ALOGE("Can't read scaling governor.");
        sun4i->boostpulse_warned = 1;

        return;
    }
    
    memcpy(current_governor, governor, MAX_BUF_SZ);

    if (strncmp(governor, "interactive", sizeof(governor)) == 0) {

        /*
         * cpufreq interactive governor: timer 20ms, min sample 60ms,
         * hispeed 700MHz at load 50%.
         */

        sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_rate",
                    "20000");
        sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/min_sample_time",
                    "60000");
        sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq",
                    "696000");
        sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load",
                    "50");
        sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay",
                    "100000");

    } else if (strncmp(governor, "ondemand", sizeof(governor)) == 0) {

        /*
         * cpufreq ondemand governor: boostfreq 696MHz, up threshold 70%,
         * sampling rate 50000.
         */

        sysfs_write("/sys/devices/system/cpu/cpufreq/ondemand/boostfreq",
                    "696000");
        sysfs_write("/sys/devices/system/cpu/cpufreq/ondemand/up_threshold",
                    "70");
        sysfs_write("/sys/devices/system/cpu/cpufreq/ondemand/sampling_rate",
                    "50000");
    }
    
}

static int boostpulse_open(struct sun4i_power_module *sun4i)
{
    struct power_module *module;
    char buf[80];
    char governor[80];

    pthread_mutex_lock(&sun4i->lock);

    if (sun4i->boostpulse_fd < 0) {
        if (get_scaling_governor(governor, sizeof(governor)) < 0) {
            ALOGE("Can't read scaling governor.");
            sun4i->boostpulse_warned = 1;

        } else {
            if (strncmp(governor, current_governor, strlen(governor)) != 0)
                sun4i_power_init(module);
                
            if (strncmp(governor, "interactive", sizeof(governor)) == 0)
                sun4i->boostpulse_fd = open(BOOSTPULSE_INTERACTIVE, O_WRONLY);
            else if (strncmp(governor, "ondemand", sizeof(governor)) == 0)
                sun4i->boostpulse_fd = open(BOOSTPULSE_ONDEMAND, O_WRONLY);

            if (sun4i->boostpulse_fd < 0 && !sun4i->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening boostpulse: %s\n", buf);
                sun4i->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&sun4i->lock);
    return sun4i->boostpulse_fd;
}

static void sun4i_power_set_interactive(struct power_module *module, int on)
{
    int len;
    char buf[MAX_BUF_SZ];

    /*
     * Lower maximum frequency when screen is off.
     */

    if (!on) {
        /* read the current scaling max freq and save it before updating */
        len = sysfs_read(SCALINGMAXFREQ, buf, sizeof(buf));

        /* make sure it's not the screen off freq, if the "on"
         * call is skipped (can happen if you press the power
         * button repeatedly) we might have read it. We should
         * skip it if that's the case
         */
        if (len != -1 && strncmp(buf, screen_off_max_freq,
                strlen(screen_off_max_freq)) != 0)
            memcpy(scaling_max_freq, buf, sizeof(buf));
        sysfs_write(SCALINGMAXFREQ, screen_off_max_freq);
    } else
        sysfs_write(SCALINGMAXFREQ, scaling_max_freq);
}

static void sun4i_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct sun4i_power_module *sun4i = (struct sun4i_power_module *) module;
    char buf[80];
    int len;
    int duration = 1;
    
    switch (hint) {
    case POWER_HINT_INTERACTION:
	if (boostpulse_open(sun4i) >= 0) {
            if (data != NULL)
	        duration = (int) data;

	    snprintf(buf, sizeof(buf), "%d", duration);
	    len = write(sun4i->boostpulse_fd, buf, strlen(buf));

	    if (len < 0) {
	        strerror_r(errno, buf, sizeof(buf));
	        ALOGE("Error writing to boostpulse: %s\n", buf);

                pthread_mutex_lock(&sun4i->lock);
                close(sun4i->boostpulse_fd);
                sun4i->boostpulse_fd = -1;
                sun4i->boostpulse_warned = 0;
                pthread_mutex_unlock(&sun4i->lock);
	    }

	}
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct sun4i_power_module HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = POWER_MODULE_API_VERSION_0_2,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = POWER_HARDWARE_MODULE_ID,
            .name = "SUN4I Power HAL",
            .author = "The Android Open Source Project",
            .methods = &power_module_methods,
        },

       .init = sun4i_power_init,
       .setInteractive = sun4i_power_set_interactive,
       .powerHint = sun4i_power_hint,
    },

    .lock = PTHREAD_MUTEX_INITIALIZER,
    .boostpulse_fd = -1,
    .boostpulse_warned = 0,
};
