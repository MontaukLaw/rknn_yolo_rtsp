hide := @
ECHO := echo

SDK_ROOT = /home/marc/rv1126_rv1109_linux_sdk_v1.8.0_20210224

GCC := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
G++ := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++

SYSROOT = /home/marc/rv1126_rv1109_linux_sdk_v1.8.0_20210224/buildroot/output/rockchip_rv1126_rv1109/host/arm-buildroot-linux-gnueabihf/sysroot

CFLAGS := -I../../include/rkmedia \
		-I$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rknpu-1.5.0/rknn/rknn_api/librknn_api/include/ \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/common \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/xcore \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/uAPI \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/algos \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/iq_parser \
		-I./include \
		-I/usr/include/ \
		-I/usr/arm-linux-gnueabihf/include/

LIB_FILES := -L$(SYSROOT)/usr/lib \
			 -L$(SDK_ROOT)/external/rkmedia/examples/librtsp/\
			 -L$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rockx/sdk/rockx-rk1806-Linux/lib/

LD_FLAGS := -lpthread -leasymedia -ldrm -lrockchip_mpp \
	        -lavformat -lavcodec -lswresample -lavutil \
			-lasound -lv4l2 -lv4lconvert -lrga \
			-lRKAP_ANR -lRKAP_Common -lRKAP_3A \
			-lmd_share -lrkaiq -lod_share -lrtsp -lrknn_api

CFLAGS += -DRKAIQ

SAMPLE_COMMON := ../common/sample_common_isp.c

all:
	$(G++) main.cc comm.cc rknn_funcs.cpp postprocess.cc $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rknn_yolo_rtsp --sysroot=$(SYSROOT)
	# $(GCC) rknn_yolo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rknn_yolo --sysroot=$(SYSROOT)
	# $(GCC) gpio_controll.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/gpio_controll --sysroot=$(SYSROOT)
	# $(GCC) my_vi_rga_venc_rtsp_demo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/my_vi_rga_venc_rtsp_demo --sysroot=$(SYSROOT)
	# $(GCC) my_vi_venc_rtsp_demo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/my_vi_venc_rtsp_demo --sysroot=$(SYSROOT)
	# $(GCC) rkmedia_vi_rknn_venc_rtsp_test.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rkmedia_vi_rknn_venc_rtsp_test --sysroot=$(SYSROOT)
	# $(GCC) rkmedia_vi_venc_rtsp_test.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rkmedia_vi_venc_rtsp_test --sysroot=$(SYSROOT)
	# $(GCC) helloworld.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/helloworld --sysroot=$(SYSROOT)
	$(hide)$(ECHO) "Build Done ..."

