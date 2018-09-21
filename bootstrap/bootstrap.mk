$(call inherit-product, $(PRODUCT_DIR)bootstrap_armv7a_neon.mk)

PRODUCT_COPY_FILES += \
	$(PRODUCT_DIR)sama5d2-xplained/init.bootstrap.rc:root/init.bootstrap.rc \
	$(PRODUCT_DIR)sama5d2-xplained/ueventd.bootstrap.rc:root/ueventd.bootstrap.rc

# product override
PRODUCT_NAME := bootstrap
PRODUCT_DEVICE := sama5d2-xplained
PRODUCT_BRAND := Lampoo
PRODUCT_MODEL := Bootstrap on sama5d2

# don't perform API checks
WITHOUT_CHECK_API := true
