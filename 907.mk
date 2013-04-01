#
# Copyright (C) 2011 The Android Open-Source Project
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

PRODUCT_COPY_FILES := \
	device/softwinner/907/kernel:kernel \
  device/softwinner/907/modules:system/lib/modules \
  device/softwinner/907/modules/ft5x_ts.ko:root/lib/modules/ft5x_ts.ko \
  device/softwinner/907/prebuilt/bin/reboot-recovery.sh:root/sbin/reboot-recovery.sh \
	device/softwinner/907/init.rc:root/init.rc \
	device/softwinner/907/initlogo.rle:root/initlogo.rle \
	device/softwinner/907/init.sun4i.rc:root/init.sun4i.rc \
	device/softwinner/907/init.sun4i.usb.rc:root/init.sun4i.usb.rc \
	device/softwinner/907/ueventd.sun4i.rc:root/ueventd.sun4i.rc

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PROPERTY_OVERRIDES += \
	ro.opengles.version = 131072 \
  ro.opengles.surface.rgb565=true \
	debug.egl.hw=1 \
	ro.carrier=wifi-only \
	ro.display.switch=1 \
	ro.sf.lcd_density=160 \
	hwui.render_dirty_regions=false \
	wifi.interface = wlan0 \
	wifi.supplicant_scan_interval = 180 \
	persist.sys.strictmode.visual=0 \
	persist.sys.strictmode.disable=1 \
	persist.sys.usb.config=mass_storage,adb \
	dalvik.vm.verify-bytecode=false \
	dalvik.vm.dexopt-flags=v=n,o=v \
	dalvik.vm.execution-mode=int:jit \
	persist.sys.timezone=Europe/London \
	persist.sys.language=en \
	persist.sys.country=GB \
	ro.com.google.locationfeatures=1 \
	dalvik.vm.lockprof.threshold=500 \
	ro.kernel.android.checkjni=0 \
	dalvik.vm.checkjni=false \
	dalvik.vm.dexopt-data-only=1 \
	ro.vold.umsdirtyratio=20 \
	persist.sys.use_dithering=0 \
	persist.sys.purgeable_assets=0 \
	windowsmgr.max_events_per_sec=240 \
	view.touch_slop=2 \
	view.minimum_fling_velocity=25 \
	ro.additionalmounts=/storage/sdcard1 \
	ro.vold.switchablepair=/storage/sdcard0,/storage/sdcard1 \
	persist.sys.vold.switchexternal=0 \
	debug.sf.hw=1 \
        rild.libpath=/system/lib/liballwinner-ril.so \
        rild.libargs=-d /dev/ttyUSB2 \
        keyguard.no_require_sim=true \
        ro.telephony.ril.v3=skipnullaid,skippinpukcount,skipbrokendatacall \
	logcat.live=disable \
	ro.config.nocheckin=1

DEVICE_PACKAGE_OVERLAYS := device/softwinner/907/overlay

# Permissions
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_PACKAGES += \
	librs_jni

# Hardware libs
PRODUCT_PACKAGES += \
	audio.primary.exDroid \
	audio.a2dp.default \
	audio.usb.default \
	display.sun4i \
	gralloc.sun4i \
	power.sun4i \
	hwcomposer.exDroid \
	lights.sun4i \
  libcedarxbase \
  libcedarxosal \
	libcedarv \
	libcedarv_adapter \
  libcedarv_base \
	libCedarA \
	libCedarX \
	libstagefright_soft_cedar_h264dec \
  libswdrm \
	libaw_audio \
	libaw_audioa \
	libfacedetection \
	librtmp \
	libve \
	libaudioutils \
	chat \
	u3gmonitor \
	devlistener

# CyanogenMOD
PRODUCT_PACKAGES += \
	com.android.future.usb.accessory

# EXT4 Support
PRODUCT_PACKAGES += \
	make_ext4fs \
	e2fsck

$(call inherit-product, frameworks/native/build/tablet-dalvik-heap.mk)
$(call inherit-product, build/target/product/full_base.mk)

# Should be after the full_base include, which loads languages_full
PRODUCT_AAPT_CONFIG := normal xhdpi mdpi
PRODUCT_AAPT_PREF_CONFIG := mdpi

PRODUCT_NAME := full_907
PRODUCT_DEVICE := 907

TARGET_SCREEN_HEIGHT := 768
TARGET_SCREEN_WIDTH := 1024
