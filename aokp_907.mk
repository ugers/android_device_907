# Inherit device configuration
$(call inherit-product, device/softwinner/907/full_907.mk)
$(call inherit-product, device/softwinner/907/907-blobs.mk)

# Inherit AOKP common bits
$(call inherit-product, vendor/aokp/configs/common.mk)

#skip asserts for now
TARGET_OTA_ASSERT_SKIP := true

# Inherit GSM common stuff
$(call inherit-product, vendor/aokp/configs/gsm.mk)

# Tablets Overlay
PRODUCT_PACKAGE_OVERLAYS += vendor/aokp/overlay/common_tablets


## Device identifier. This must come after all inclusions
PRODUCT_NAME := aokp_907
PRODUCT_BRAND := Google
PRODUCT_DEVICE := 907
PRODUCT_MODEL := Nexus 7
PRODUCT_MANUFACTURER := Asus

#Set build fingerprint / ID / Product Name ect.
PRODUCT_BUILD_PROP_OVERRIDES += PRODUCT_NAME=nakasi TARGET_DEVICE=grouper BUILD_FINGERPRINT="google/nakasi/grouper:4.4.4/JDQ39E/402395:user/release-keys" PRIVATE_BUILD_DESC="nakasi-user 4.4.4 JDQ39E 402395 release-keys"

# Allow ADB (to access dev settings)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.secure=0 service.adb.enable=1 persist.sys.usb.config=mass_storage ro.debuggable=1 persist.service.adb.enable=1 rild.libpath=/system/lib/liballwinner-ril.so

PRODUCT_COPY_FILES += \
    vendor/aokp/prebuilt/bootanimation/bootanimation_1024_768.zip:system/media/bootanimation-alt.zip
