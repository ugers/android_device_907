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
	device/softwinner/907/init.recovery.sun4i.rc:root/init.recovery.sun4i.rc \
	device/softwinner/907/init.sun4i.rc:root/init.sun4i.rc \
        device/softwinner/907/fstab.sun4i:root/fstab.sun4i \
	device/softwinner/907/init.sun4i.usb.rc:root/init.sun4i.usb.rc \
	device/softwinner/907/ueventd.sun4i.rc:root/ueventd.sun4i.rc
	
# /system/bin
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/bin/fsck.exfat:system/bin/fsck.exfat \
	device/softwinner/907/prebuilt/bin/mkfs.exfat:system/bin/mkfs.exfat \
	device/softwinner/907/prebuilt/bin/mount.exfat:system/bin/mount.exfat \
	device/softwinner/907/prebuilt/bin/ntfs-3g:system/bin/ntfs-3g \
	device/softwinner/907/prebuilt/bin/ntfs-3g.probe:system/bin/ntfs-3g.probe \
	device/softwinner/907/prebuilt/bin/mkntfs:system/bin/mkntfs \
	device/softwinner/907/prebuilt/bin/reboot-recovery.sh:system/bin/reboot-recovery.sh \
	device/softwinner/907/prebuilt/bin/usb_modeswitch:system/bin/usb_modeswitch

# /system/etc
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/etc/ppp/ip-down:system/etc/ppp/ip-down \
	device/softwinner/907/prebuilt/etc/ppp/ip-up:system/etc/ppp/ip-up \
	device/softwinner/907/prebuilt/etc/ppp/call-pppd:system/etc/ppp/call-pppd \
	device/softwinner/907/prebuilt/etc/firmware/ath3k-1.fw:system/etc/firmware/ath3k-1.fw \
	device/softwinner/907/prebuilt/etc/firmware/ath3k-1.fw:system/vendor/firmware/ath3k-1.fw \
	device/softwinner/907/prebuilt/etc/3g_dongle.cfg:system/etc/3g_dongle.cfg \
	device/softwinner/907/prebuilt/etc/camera.cfg:system/etc/camera.cfg \
	device/softwinner/907/prebuilt/etc/gps.conf:system/etc/gps.conf \
	device/softwinner/907/prebuilt/etc/usb_modeswitch.sh:system/etc/usb_modeswitch.sh \
	device/softwinner/907/prebuilt/etc/vold.fstab:system/etc/vold.fstab

# Media files
PRODUCT_COPY_FILES += \
    device/softwinner/907/prebuilt/etc/media_codecs.xml:system/etc/media_codecs.xml \
    device/softwinner/907/prebuilt/etc/media_profiles.xml:system/etc/media_profiles.xml \
    device/softwinner/907/prebuilt/etc/audio_effects.conf:system/etc/audio_effects.conf

# NAND tends to be slow, lets preload some things
PRODUCT_COPY_FILES += device/softwinner/907/prebuilt/etc/init.d/01preload:system/etc/init.d/01preload

PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,device/softwinner/907/prebuilt/etc/init.d,system/etc/init.d)

PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,device/softwinner/907/prebuilt/etc/usb_modeswitch.d,system/etc/usb_modeswitch.d)

# /system/lib
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/lib/egl/libEGL_mali.so:system/lib/egl/libEGL_mali.so \
	device/softwinner/907/prebuilt/lib/egl/libGLESv1_CM_mali.so:system/lib/egl/libGLESv1_CM_mali.so \
	device/softwinner/907/prebuilt/lib/egl/libGLESv2_mali.so:system/lib/egl/libGLESv2_mali.so \
	device/softwinner/907/prebuilt/lib/liballwinner-ril.so:system/lib/liballwinner-ril.so \
	device/softwinner/907/prebuilt/lib/hw/sensors.exDroid.so:system/lib/hw/sensors.exDroid.so \
	device/softwinner/907/prebuilt/lib/libMali.so:system/lib/libMali.so \
	device/softwinner/907/prebuilt/lib/libUMP.so:system/lib/libUMP.so \
	device/softwinner/907/prebuilt/lib/libMali.so:obj/lib/libMali.so \
	device/softwinner/907/prebuilt/lib/libUMP.so:obj/lib/libUMP.so

# /system/usr
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/usr/idc/ft5x_ts.idc:system/usr/idc/ft5x_ts.idc \
	device/softwinner/907/prebuilt/usr/idc/gt80x.idc:system/usr/idc/gt80x.idc \
	device/softwinner/907/prebuilt/usr/idc/gt811_ts.idc:system/usr/idc/gt811_ts.idc \
	device/softwinner/907/prebuilt/usr/idc/ssd253x.idc:system/usr/idc/ssd253x.idc \
	device/softwinner/907/prebuilt/usr/idc/Goodix-TS-board-3.idc:system/usr/idc/Goodix-TS-board-3.idc \
	device/softwinner/907/prebuilt/usr/idc/Goodix-TS.idc:system/usr/idc/Goodix-TS.idc \
	device/softwinner/907/prebuilt/usr/idc/gt82x.idc:system/usr/idc/gt82x.idc \
	device/softwinner/907/prebuilt/usr/idc/novatek-ts.idc:system/usr/idc/novatek-ts.idc \
	device/softwinner/907/prebuilt/usr/idc/pixcir_cxx.idc:system/usr/idc/pixcir_cxx.idc \
	device/softwinner/907/prebuilt/usr/idc/sun4i-ts.idc:system/usr/idc/sun4i-ts.idc \
	device/softwinner/907/prebuilt/usr/keylayout/axp20-supplyer.kl:system/usr/keylayout/axp20-supplyer.kl \
	device/softwinner/907/prebuilt/usr/keylayout/sun4i-keyboard.kl:system/usr/keylayout/sun4i-keyboard.kl \
	device/softwinner/907/prebuilt/usr/keylayout/hv_keypad.kl:system/usr/keylayout/hv_keypad.kl \
	device/softwinner/907/prebuilt/usr/icu/icudt46l.dat:system/usr/icu/icudt46l.dat

# temporary prebuilt wpa_supplicant
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/etc/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf

# prebuilt kernel modules
#PRODUCT_COPY_FILES += \
#	$(call find-copy-subdir-files,*,device/softwinner/907/prebuilt/vendor/modules,system/vendor/modules)

PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,device/softwinner/907/prebuilt/lib/modules,system/lib/modules)

# HACK by turl: Create some intermediate files to link with libMali/libUMP
$(shell mkdir -p out/target/product/907/obj/SHARED_LIBRARIES/libMali_intermediates)
$(shell mkdir -p out/target/product/907/obj/SHARED_LIBRARIES/libUMP_intermediates)
$(shell touch out/target/product/907/obj/SHARED_LIBRARIES/libMali_intermediates/export_includes)
$(shell touch out/target/product/907/obj/SHARED_LIBRARIES/libUMP_intermediates/export_includes)

# Audio
PRODUCT_COPY_FILES += \
        device/softwinner/907/libraries/audio/audio_policy.conf:system/etc/audio_policy.conf

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PROPERTY_OVERRIDES += \
	ro.sys.bootfast=true \
	ro.eventproc.start=0 \
        ro.debuggable=1 \
	ro.com.google.locationfeatures=1 \
	ro.kernel.android.checkjni=0 \
	ro.vold.umsdirtyratio=20 \
	ro.media.dec.jpeg.memcap=20000000 \
	ro.additionalmounts=/storage/sdcard1 \
	ro.vold.switchablepair=/storage/sdcard0,/storage/sdcard1 \
	ro.config.nocheckin=1 \
	debug.sf.hw=1 \
	drm.service.enabled=true
	dalvik.vm.lockprof.threshold=500 \
	dalvik.vm.checkjni=false \
	dalvik.vm.dexopt-data-only=1 \
	persist.sys.vold.switchexternal=0 \
        persist.service.adb.enable=1 \
        keyguard.no_require_sim=true \
	logcat.live=disable \
	net.dns1=8.8.8.8 \
	net.dns2=8.8.4.4 \

DEVICE_PACKAGE_OVERLAYS := device/softwinner/907/overlay

# Permissions
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
        frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_COPY_FILES += \
	system/bluetooth/data/audio.conf:system/etc/bluetooth/audio.conf \
	system/bluetooth/data/auto_pairing.conf:system/etc/bluetooth/auto_pairing.conf \
	system/bluetooth/data/blacklist.conf:system/etc/bluetooth/blacklist.conf \
	system/bluetooth/data/input.conf:system/etc/bluetooth/input.conf \
	system/bluetooth/data/network.conf:system/etc/bluetooth/network.conf \

#Cedarx prebuild lib's from 4.1
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/lib/cedarx/libaw_audio.so:system/lib/libaw_audio.so \
	device/softwinner/907/prebuilt/lib/cedarx/libaw_audioa.so:system/lib/libaw_audioa.so \
	device/softwinner/907/prebuilt/lib/cedarx/libCedarA.so:system/lib/libCedarA.so \
	device/softwinner/907/prebuilt/lib/cedarx/libcedarv.so:system/lib/libcedarv.so \
	device/softwinner/907/prebuilt/lib/cedarx/libcedarv_adapter.so:system/lib/libcedarv_adapter.so \
	device/softwinner/907/prebuilt/lib/cedarx/libcedarv_base.so:system/lib/libcedarv_base.so \
	device/softwinner/907/prebuilt/lib/cedarx/libCedarX.so:system/lib/libCedarX.so \
	device/softwinner/907/prebuilt/lib/cedarx/libcedarxbase.so:system/lib/libcedarxbase.so  \
	device/softwinner/907/prebuilt/lib/cedarx/libcedarxosal.so:system/lib/libcedarxosal.so \
	device/softwinner/907/prebuilt/lib/cedarx/libfacedetection.so:system/lib/libfacedetection.so \
	device/softwinner/907/prebuilt/lib/cedarx/librtmp.so:system/lib/librtmp.so \
	device/softwinner/907/prebuilt/lib/cedarx/libstagefright_soft_cedar_h264dec.so:system/lib/libstagefright_soft_cedar_h264dec.so \
	device/softwinner/907/prebuilt/lib/cedarx/libswdrm.so:system/lib/libswdrm.so \
	device/softwinner/907/prebuilt/lib/cedarx/libve.so:system/lib/libve.so \
	
#Cedarx-Crack
PRODUCT_COPY_FILES += \
	device/softwinner/907/prebuilt/lib/cedarx/libdemux_rmvb.so:system/lib/libdemux_rmvb.so \
	device/softwinner/907/prebuilt/lib/cedarx/librm.so:system/lib/librm.so \
	device/softwinner/907/prebuilt/lib/cedarx/libswa1.so:system/lib/libswa1.so \
	device/softwinner/907/prebuilt/lib/cedarx/libswa2.so:system/lib/libswa2.so \

#Wlan
PRODUCT_PACKAGES += \
	libnetcmdiface

# Device specific settings
PRODUCT_PACKAGES += \
	AllwinnerA10Settings

PRODUCT_PACKAGES += \
	librs_jni \
        rild_sun4i

# Bluetooth
PRODUCT_PACKAGES += \
	bluetoothd \
	hciconfig \
	hcitool


# Hardware libs
PRODUCT_PACKAGES += \
	audio.primary.exDroid \
	audio.a2dp.default \
	audio.usb.default \
	display.sun4i \
	gralloc.sun4i \
	power.sun4i \
	camera.exDroid \
	hwcomposer.exDroid \
	lights.sun4i \
	libaudioutils \
	chat \
	u3gmonitor \
	devlistener

PRODUCT_PACKAGES += \
	libthirdpartstream \
	libcedarxsftstream \
	libsrec_jni \
	libjpgenc \
	libI420colorconvert.so \
	libstagefrighthw \
	libOmxCore \
	libOmxVdec \
	libOmxVenc \
	libaw_h264enc \
	
# CedarX libraries
PRODUCT_PACKAGES += \
	libCedarA \
	libCedarX \
	libcedarv \
        libcedarxbase \
        libcedarxosal \
        libswdrm \
	libcedarv_adapter \
	libve \
	libfacedetection \
	libaw_audio \
	libaw_audioa \
        libcedarv_base \
	libstagefright_soft_cedar_h264dec \
	librtmp

# CyanogenMOD
PRODUCT_PACKAGES += \
	com.android.future.usb.accessory

# EXT4 Support
PRODUCT_PACKAGES += \
	make_ext4fs \
	e2fsck \
	cpueater

$(call inherit-product, frameworks/native/build/tablet-dalvik-heap.mk)
$(call inherit-product, build/target/product/full_base.mk)

# Should be after the full_base include, which loads languages_full
PRODUCT_AAPT_CONFIG := large xlarge hdpi mdpi
PRODUCT_AAPT_PREF_CONFIG := mdpi

PRODUCT_NAME := full_907
PRODUCT_DEVICE := 907

TARGET_SCREEN_HEIGHT := 768
TARGET_SCREEN_WIDTH := 1024
