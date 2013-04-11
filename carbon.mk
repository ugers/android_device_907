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

$(call inherit-product, device/softwinner/907/full_907.mk)
$(call inherit-product, device/softwinner/907/907-blobs.mk)

# Inherit some common CarbonDev stuff.
$(call inherit-product, vendor/carbon/config/common_tablet.mk)

# Inherit GSM common stuff
$(call inherit-product, vendor/carbon/config/common_gsm.mk)

## Device identifier. This must come after all inclusions
PRODUCT_NAME := carbon_907
PRODUCT_BRAND := Google
PRODUCT_DEVICE := 907
PRODUCT_MODEL := Nexus 7
PRODUCT_MANUFACTURER := Asus
PRODUCT_RELEASE_NAME := 907

UTC_DATE := $(shell date +%s)
DATE     := $(shell date +%Y%m%d)

PRODUCT_LOCALES := en_US fr_FR it_IT es_ES de_DE nl_NL cs_CZ pl_PL ja_JP zh_TW zh_CN ru_RU ko_KR nb_NO nn_NO es_US da_DK el_GR tr_TR pt_PT pt_BR rm_CH sv_SE bg_BG ca_ES en_GB fi_FI hi_IN hr_HR hu_HU in_ID iw_IL ka_GE lt_LT lv_LV ro_RO sk_SK sl_SI sr_RS uk_UA vi_VN tl_PH ar_EG fa_IR th_TH sw_TZ ms_MY af_ZA zu_ZA am_ET hi_IN

#Set build fingerprint / ID / Product Name ect.
PRODUCT_BUILD_PROP_OVERRIDES += PRODUCT_NAME=nakasi TARGET_DEVICE=grouper BUILD_FINGERPRINT="google/nakasi/grouper:4.2.2/JDQ39/402395:user/release-keys" PRIVATE_BUILD_DESC="nakasi-user 4.2.2 JDQ39 402395 release-keys"

# Allow ADB (to access dev settings)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.debuggable=1 persist.sys.usb.config=mass_storage persist.service.adb.enable=1
