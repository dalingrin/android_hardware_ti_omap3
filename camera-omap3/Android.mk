ifdef BOARD_USES_TI_CAMERA_HAL

################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    CameraHal.cpp \
    CameraHal_Utils.cpp \
    MessageQueue.cpp \
    
LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \

LOCAL_C_INCLUDES += \
	kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay

LOCAL_CFLAGS += -fno-short-enums 

ifdef HARDWARE_OMX

LOCAL_SRC_FILES += \
    scale.c \
    JpegEncoder.cpp \
    JpegEncoderEXIF.cpp \

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/dspbridge/api/inc \
	hardware/ti/omx/system/src/openmax_il/lcml/inc \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \
	hardware/ti/omx/system/src/openmax_il/common/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager_proxy/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager/resource_activity_monitor/inc \
	hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc \
	external/libexif \
	
LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX

LOCAL_SHARED_LIBRARIES += \
    libbridge \
    libLCML \
    libOMX_Core \
    libOMX_ResourceManagerProxy

LOCAL_STATIC_LIBRARIES := \
	libexifgnu

endif


ifdef FW3A

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/fw3A/include

LOCAL_SHARED_LIBRARIES += \
    libdl \

LOCAL_CFLAGS += -O0 -g3 -DIPP_LINUX -D___ANDROID___ -DFW3A -DICAP

endif

ifdef IMAGE_PROCESSING_PIPELINE

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/mm_isp/ipp/inc \
	hardware/ti/omap3/mm_isp/capl/inc \

LOCAL_SHARED_LIBRARIES += \
    libcapl \
    libImagePipeline

LOCAL_CFLAGS += -DIMAGE_PROCESSING_PIPELINE

endif

LOCAL_MODULE:= libcamera

include $(BUILD_SHARED_LIBRARY)

################################################

ifdef HARDWARE_OMX

include $(CLEAR_VARS)

LOCAL_SRC_FILES := JpegEncoderTest.cpp

LOCAL_C_INCLUDES := hardware/ti/omx/system/src/openmax_il/omx_core/inc\
                    hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc \
                    external/libexif \

LOCAL_SHARED_LIBRARIES := libcamera

LOCAL_MODULE := JpegEncoderTest

include $(BUILD_EXECUTABLE)

endif

################################################

################################################

ifdef HARDWARE_OMX
ifdef FW3A

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	camera_test.cpp

LOCAL_SHARED_LIBRARIES:= \
	libdl \
	libui \
	libutils \
	libcutils \
	libcamera

LOCAL_C_INCLUDES += \
    hardware/ti/omap3/fw3A/include \
	hardware/ti/omx/system/src/openmax_il/lcml/inc \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \
	hardware/ti/omx/system/src/openmax_il/common/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager_proxy/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager/resource_activity_monitor/inc \
	hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc \
	external/libexif \
	hardware/ti/omap3/liboverlay \
	frameworks/base/include/ui \

LOCAL_MODULE:= camera_test

LOCAL_CFLAGS += -Wall -fno-short-enums -O0 -g -D___ANDROID___ -DMMS_CAMERA_TEST -DFW3A -DICAP -DIPP_LINUX -DHARDWARE_OMX -DIPP_LINUX

include $(BUILD_EXECUTABLE)

endif
endif

################################################


endif
