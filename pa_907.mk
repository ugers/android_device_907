# Copyright (C) 2012 ParanoidAndroid Project
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

# Define PA bootanimation size
PARANOID_BOOTANIMATION_NAME := XHDPI

# OVERLAY_TARGET adds overlay asset source
OVERLAY_TARGET := pa_hdpi

# Build paprefs from sources
PREFS_FROM_SOURCE ?= false

# Build paprefs from sources
DEVELOPER_VERSION ?= false

#skip asserts for now
TARGET_OTA_ASSERT_SKIP := true

# CM Package Extras
-include vendor/pa/packages/cm.mk

# Include ParanoidAndroid common configuration
include vendor/pa/main.mk

# Inherit device configuration
$(call inherit-product, device/softwinner/907/full_907.mk)
$(call inherit-product, device/softwinner/907/907-blobs.mk)

# Override AOSP build properties
PRODUCT_NAME := pa_907
PRODUCT_BRAND := google
PRODUCT_MODEL := Nexus 10
PRODUCT_MANUFACTURER := Samsung
PRODUCT_BUILD_PROP_OVERRIDES += PRODUCT_NAME=mantaray BUILD_FINGERPRINT="google/mantaray/manta:4.4.4/JDQ39/573038:user/release-keys" PRIVATE_BUILD_DESC="mantaray-user 4.4.4 JDQ39 573038 release-keys"

# Allow ADB (to access dev settings)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.secure=0 service.adb.enable=1 persist.sys.usb.config=mass_storage ro.debuggable=1 persist.service.adb.enable=1 rild.libpath=/system/lib/liballwinner-ril.so
