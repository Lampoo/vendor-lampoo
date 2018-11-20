LOCAL_PATH:= $(call my-dir)

# Set up common variables for usage across the libjpeg-turbo modules

# By default, the build system generates ARM target binaries in thumb mode,
# where each instruction is 16 bits wide.  Defining this variable as arm
# forces the build system to generate object files in 32-bit arm mode.  This
# is the same setting previously used by libjpeg and it is still benefitial.
libjpeg_turbo_common_arm_mode := arm

libjpeg_turbo_common_cflags := -O3 -fstrict-aliasing
libjpeg_turbo_common_cflags += -Wno-unused-parameter -Wno-sign-compare -Werror

libjpeg_turbo_common_src_files := \
    jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
    jcicc.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c \
    jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c \
    jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdicc.c jdinput.c \
    jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c \
    jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c \
    jidctint.c jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c

# uncomment if WITH_ARITH_ENC OR WITH_ARITH_DEC
libjpeg_turbo_common_src_files += jaricom.c

# uncomment if WITH_ARITH_ENC
libjpeg_turbo_common_src_files += jcarith.c

# uncomment if WITH_ARITH_DEC
libjpeg_turbo_common_src_files += jdarith.c

# SIMD handling
# TARGET_ARCH = { arm | arm64 | x86 | x86_64 | mips | mips64 }

# ARM v7 (NEON)
ifeq ($(strip $(TARGET_ARCH)),arm)
  ifeq ($(ARCH_ARM_HAVE_NEON),true)
    libjpeg_turbo_simd_src_files := simd/arm/jsimd_neon.S simd/arm/jsimd.c
  endif
endif

# ARM v8 64-bit NEON
ifeq ($(strip $(TARGET_ARCH)),arm64)
  libjpeg_turbo_simd_src_files := simd/arm64/jsimd_neon.S simd/arm64/jsimd.c
endif

# x86 MMX and SSE2
ifeq ($(strip $(TARGET_ARCH)),x86)
  libjpeg_turbo_simd_src_files := \
      jccolext-avx2.asm jcgray-sse2.asm jcsample-sse2.asm jdmerge-mmx.asm jfdctflt-3dn.asm jidctflt-sse2.asm \
      jidctred-sse2.asm jsimdcpu.asm jccolext-mmx.asm jcgryext-avx2.asm jdcolext-avx2.asm jdmerge-sse2.asm \
      jfdctflt-sse.asm jidctflt-sse.asm jquant-3dn.asm jccolext-sse2.asm jcgryext-mmx.asm jdcolext-mmx.asm \
      jdmrgext-avx2.asm jfdctfst-mmx.asm jidctfst-mmx.asm jquantf-sse2.asm jccolor-avx2.asm jcgryext-sse2.asm \
      jdcolext-sse2.asm jdmrgext-mmx.asm jfdctfst-sse2.asm jidctfst-sse2.asm jquanti-avx2.asm jccolor-mmx.asm \
      jchuff-sse2.asm jdcolor-avx2.asm jdmrgext-sse2.asm jfdctint-avx2.asm jidctint-avx2.asm jquanti-sse2.asm \
      jccolor-sse2.asm jcphuff-sse2.asm jdcolor-mmx.asm jdsample-avx2.asm jfdctint-mmx.asm jidctint-mmx.asm \
      jquant-mmx.asm jcgray-avx2.asm jcsample-avx2.asm jdcolor-sse2.asm jdsample-mmx.asm jfdctint-sse2.asm \
      jidctint-sse2.asm jquant-sse.asm jcgray-mmx.asm jcsample-mmx.asm jdmerge-avx2.asm jdsample-sse2.asm \
      jidctflt-3dn.asm jidctred-mmx.asm jsimd.c
  libjpeg_turbo_simd_src_files := $(addprefix simd/i386/,$(libjpeg_turbo_simd_src_files))
  libjpeg_turbo_common_asflags := -DPIC -DELF
  libjpeg_turbo_common_includes := $(LOCAL_PATH)/simd $(LOCAL_PATH)/simd/nasm
endif

# x86_64 MMX and SSE2
ifeq ($(strip $(TARGET_ARCH)),x86_64)
  libjpeg_turbo_simd_src_files := \
      jccolext-avx2.asm jcgray-sse2.asm jcsample-avx2.asm jdcolor-sse2.asm jdsample-avx2.asm jfdctint-sse2.asm \
      jidctred-sse2.asm jsimdcpu.asm jccolext-sse2.asm jcgryext-avx2.asm jcsample-sse2.asm jdmerge-avx2.asm 
      jdsample-sse2.asm jidctflt-sse2.asm jquantf-sse2.asm jccolor-avx2.asm jcgryext-sse2.asm jdcolext-avx2.asm \
      jdmerge-sse2.asm jfdctflt-sse.asm jidctfst-sse2.asm jquanti-avx2.asm jccolor-sse2.asm jchuff-sse2.asm \
      jdcolext-sse2.asm jdmrgext-avx2.asm jfdctfst-sse2.asm jidctint-avx2.asm jquanti-sse2.asm jcgray-avx2.asm \
      jcphuff-sse2.asm jdcolor-avx2.asm jdmrgext-sse2.asm jfdctint-avx2.asm jidctint-sse2.asm jsimd.c
  libjpeg_turbo_simd_src_files := $(addprefix simd/x86_64/, $(libjpeg_turbo_simd_src_files))
  libjpeg_turbo_common_asflags := -DPIC -DELF -D__x86_64__
  libjpeg_turbo_common_includes := $(LOCAL_PATH)/simd $(LOCAL_PATH)/simd/nasm
endif

# mips
ifeq ($(strip $(TARGET_ARCH)),mips)
  libjpeg_turbo_simd_src_files := simd/mips/jsimd_dspr2.S simd/mips/jsimd.c
endif

ifndef libjpeg_turbo_simd_src_files
  libjpeg_turbo_simd_src_files := jsmid_none.c
endif

include $(CLEAR_VARS)
# Build as a static library.
LOCAL_MODULE := libjpeg-turbo_static
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := $(libjpeg_turbo_common_arm_mode)
LOCAL_SRC_FILES := $(libjpeg_turbo_common_src_files) $(libjpeg_turbo_simd_src_files)
# cflags
LOCAL_CFLAGS := $(libjpeg_turbo_common_cflags)
LOCAL_C_INCLUDES := $(libjpeg_turob_common_includes)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
ifneq (,$(TARGET_BUILD_APPS))
  # Unbundled branch, built against NDK.
  LOCAL_SDK_VERSION := 17
endif
include $(BUILD_STATIC_LIBRARY)

# Also build as a shared library.
include $(CLEAR_VARS)
LOCAL_MODULE := libjpeg-turbo
LOCAL_WHOLE_STATIC_LIBRARIES = libjpeg-turbo_static
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
ifneq (,$(TARGET_BUILD_APPS))
  # Unbundled branch, built against NDK.
  LOCAL_SDK_VERSION := 17
endif
include $(BUILD_SHARED_LIBRARY)

# Unset all created common variables
libjpeg_turbo_common_arm_mode :=
libjpeg_turbo_common_src_files :=
libjpeg_turbo_common_asflags :=
libjpeg_turbo_common_cflags :=
libjpeg_turbo_common_includes :=
libjpeg_turbo_simd_src_files :=
