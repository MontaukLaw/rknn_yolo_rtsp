hide := @
ECHO := echo

SDK_ROOT = /opt/atk-dlrv1126-toolchain

GCC := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
G++ := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++
# G++ := /opt/atk-dlrv1126-toolchain/usr/bin/arm-linux-gnueabihf-gcc

SYSROOT = $(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/host/arm-buildroot-linux-gnueabihf/sysroot

CFLAGS := -I$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rknpu-1.5.0/rknn/rknn_api/librknn_api/include/ \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/common \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/xcore \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/uAPI \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/algos \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/iq_parser \
		-I$(SYSROOT)/usr/include/rknn/ \
		-I./include \
   		-I$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rknpu-1.5.0/rknn/rknn_api/librknn_api/include/ \
   		-I$(SYSROOT)/usr/include/rknn/ \
     	-I./libs/common \
     	-I./libs/librga/include \
   		-I./libs/common/drm/include/libdrm 

LIB_FILES := -L$(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/arm-linux-gnueabihf/include/c++/8.3.0/ \
             -L$(SYSROOT)/usr/lib \
			 -L$(SDK_ROOT)/external/rkmedia/examples/librtsp/ \
			 -L$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rockx/sdk/rockx-rk1806-Linux/lib/ \
			 -L./lib

LD_FLAGS := -lpthread -leasymedia -ldrm -lrockchip_mpp \
			-lasound -lv4l2 -lv4lconvert -lrga \
			-lRKAP_ANR -lRKAP_Common -lRKAP_3A \
			-lmd_share -lrkaiq -lod_share -lrtsp -lrknn_api

CFLAGS += -DRKAIQ

SAMPLE_COMMON := sample_common_isp.c

all:
	$(GCC) main.cc comm.cc rknn_funcs.cpp postprocess.cc rknn_model.cc yolo.cc $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rknn_yolo_rtsp --sysroot=$(SYSROOT)
	# $(GCC) rknn_yolo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rknn_yolo --sysroot=$(SYSROOT)
	# $(GCC) gpio_controll.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/gpio_controll --sysroot=$(SYSROOT)
	# $(GCC) my_vi_rga_venc_rtsp_demo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/my_vi_rga_venc_rtsp_demo --sysroot=$(SYSROOT)
	# $(GCC) my_vi_venc_rtsp_demo.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/my_vi_venc_rtsp_demo --sysroot=$(SYSROOT)
	# $(GCC) rkmedia_vi_rknn_venc_rtsp_test.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rkmedia_vi_rknn_venc_rtsp_test --sysroot=$(SYSROOT)
	# $(GCC) rkmedia_vi_venc_rtsp_test.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/rkmedia_vi_venc_rtsp_test --sysroot=$(SYSROOT)
	# $(GCC) helloworld.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/helloworld --sysroot=$(SYSROOT)
	$(hide)$(ECHO) "Build Done ..."

clean:
	rm ./build/*
