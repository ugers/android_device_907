#
# Copyright (C) 2012 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Platform/device
TARGET_BOARD_PLATFORM := exDroid
TARGET_BOOTLOADER_BOARD_NAME := crane

USE_CAMERA_STUB := true
CAMERA_USES_SURFACEFLINGER_CLIENT_STUB := true
HAVE_HTC_AUDIO_DRIVER := true
BOARD_USES_GENERIC_AUDIO := true
BOARD_USES_GPS_TYPE := simulator

TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

#CPU stuff
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_ARCH_VARIANT_CPU := cortex-a8
TARGET_GLOBAL_CFLAGS += -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
TARGET_GLOBAL_CPPFLAGS += -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
ARCH_ARM_HAVE_TLS_REGISTER := true

#Bluetooth and Vibro stuff
BOARD_HAVE_BLUETOOTH := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/softwinner/907/bluetooth
BOARD_BLUEDROID_VENDOR_CONF := device/softwinner/907/bluetooth/vnd_generic_usb.txt
BLUETOOTH_HCI_USE_USB := true
BOARD_HAS_VIBRATOR_IMPLEMENTATION := ../../device/softwinner/907/vibrator.c

# Partition sizes; must be in decimal
TARGET_USERIMAGES_USE_EXT4 := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 33554432
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 296435456
BOARD_USERDATAIMAGE_PARTITION_SIZE := 1073741824
BOARD_FLASH_BLOCK_SIZE := 4096

#EGL stuff
BOARD_EGL_CFG := device/softwinner/907/egl.cfg
USE_OPENGL_RENDERER := true
ENABLE_WEBGL := true
BOARD_USE_SKIA_LCDTEXT := true
BOARD_EGL_NEEDS_LEGACY_FB := true
BOARD_NO_ALLOW_DEQUEUE_CURRENT_BUFFER := true

#Bootanimation stuff
TARGET_BOOTANIMATION_PRELOAD := true
TARGET_BOOTANIMATION_TEXTURE_CACHE := true
TARGET_BOOTANIMATION_USE_RGB565 := true

#CWM Recovery
#BOARD_CUSTOM_RECOVERY_KEYMAPPING := ../../device/softwinner/907/recovery_keys.c
#BOARD_UMS_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun/file"
#BOARD_UMS_2ND_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun1/file"
#TARGET_RECOVERY_INITRC := device/softwinner/907/init.recovery.sun4i.rc

#TWRP recovery
BOARD_HAS_SDCARD_INTERNAL := true
TARGET_RECOVERY_INITRC := device/softwinner/907/init.recovery.sun4i.rc
TARGET_RECOVERY_PIXEL_FORMAT := "RGB565"
DEVICE_RESOLUTION := 1024x768
TW_FLASH_FROM_STORAGE := true
TW_NO_REBOOT_BOOTLOADER := true
#TWRP_EVENT_LOGGING := true
TW_INTERNAL_STORAGE_PATH := "/emmc"
TW_INTERNAL_STORAGE_MOUNT_POINT := "emmc"
TW_EXTERNAL_STORAGE_PATH := "/sdcard"
TW_EXTERNAL_STORAGE_MOUNT_POINT := "sdcard"
BOARD_UMS_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun/file"
#BOARD_UMS_2ND_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun1/file"

# Vold stuff
BOARD_VOLD_MAX_PARTITIONS := 20
BOARD_VOLD_EMMC_SHARES_DEV_MAJOR := true
BOARD_VOLD_DISC_HAS_MULTIPLE_MAJORS := true
TARGET_USE_CUSTOM_LUN_FILE_PATH = "/sys/class/android_usb/android0/f_mass_storage/lund%d/file"
TARGET_USE_CUSTOM_SECOND_LUN_NUM := 1

#Misc stuff
TARGET_RECOVERY_PRE_COMMAND := "echo -n boot-recovery | busybox dd of=/dev/block/nandf count=1 conv=sync; sync"
BOARD_USE_LEGACY_TOUCHSCREEN := true
TARGET_HARDWARE_INCLUDE := $(TOP)/device/softwinner/907/libraries/include
TARGET_PROVIDES_INIT_RC := true
TARGET_RELEASETOOL_OTA_FROM_TARGET_SCRIPT := device/softwinner/907/releasetools/907_ota_from_target_files

# Wifi stuff
BOARD_WIFI_VENDOR                := realtek
WPA_SUPPLICANT_VERSION           := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_rtl
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_rtl
BOARD_WLAN_DEVICE                := rtl8192cu
SW_BOARD_USR_WIFI                := rtl8192cu

WIFI_DRIVER_MODULE_NAME   := 8192cu
WIFI_DRIVER_MODULE_PATH   := "/system/lib/modules/8192cu.ko"

WIFI_DRIVER_MODULE_ARG    := ""
WIFI_FIRMWARE_LOADER      := ""
WIFI_DRIVER_FW_PATH_STA   := ""
WIFI_DRIVER_FW_PATH_AP    := ""
WIFI_DRIVER_FW_PATH_P2P   := ""
WIFI_DRIVER_FW_PATH_PARAM := ""

TARGET_CUSTOM_WIFI := device/softwinner/907/wifi_realtek.c

# Beware: set only prebuilt OR source+config
TARGET_PREBUILT_KERNEL := device/softwinner/907/kernel
BOARD_KERNEL_BASE := 0x40000000
BOARD_KERNEL_CMDLINE := console=ttyS0,115200 rw init=/init loglevel=8

# G-Sensor Stuff
SW_BOARD_USES_GSENSOR_TYPE := "bma250"
SW_BOARD_GSENSOR_DIRECT_X := "true"
SW_BOARD_GSENSOR_DIRECT_Y := "true"
SW_BOARD_GSENSOR_DIRECT_Z := "true"
SW_BOARD_GSENSOR_XY_REVERT := "true"

COMMON_GLOBAL_CFLAGS += "-DICS_CAMERA_BLOB -DICS_AUDIO_BLOB"
