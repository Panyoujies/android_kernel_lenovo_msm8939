GCC_VERSION      := $(shell $(CONFIG_SHELL) $(srctree)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ccflags-y += -Idrivers/media/platform/msm/lenovo/camera/camera-driver/jpeg_10
ccflags-y += -Idrivers/media/platform/msm/lenovo/camera/camera-driver/sensor/io

obj-$(CONFIG_LENOVOB_JPEG) += msm_jpeg_dev.o msm_jpeg_sync.o msm_jpeg_core.o msm_jpeg_hw.o msm_jpeg_platform.o
