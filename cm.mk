# Inherit device configuration
$(call inherit-product, device/softwinner/907/full_907.mk)
$(call inherit-product, device/softwinner/907/907-blobs.mk)

# Inherit some common CyanogenMod stuff.
$(call inherit-product, vendor/cm/config/common_full_tablet_wifionly.mk)

# Inherit GSM common stuff
$(call inherit-product, vendor/cm/config/gsm.mk)

## Device identifier. This must come after all inclusions
PRODUCT_NAME := cm_907
PRODUCT_BRAND := Google
PRODUCT_DEVICE := 907
PRODUCT_MODEL := Nexus 7
PRODUCT_MANUFACTURER := Asus
PRODUCT_RELEASE_NAME := 907

UTC_DATE := $(shell date +%s)
DATE     := $(shell date +%Y%m%d)

#Set build fingerprint / ID / Product Name ect.
PRODUCT_BUILD_PROP_OVERRIDES += PRODUCT_NAME=nakasi TARGET_DEVICE=grouper BUILD_FINGERPRINT="google/nakasi/grouper:4.2.2/JDQ39E/402395:user/release-keys" PRIVATE_BUILD_DESC="nakasi-user 4.2.2 JDQ39E 402395 release-keys"
