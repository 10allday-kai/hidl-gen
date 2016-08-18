LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hidl_test
LOCAL_MODULE_CLASS := EXECUTABLES

LOCAL_SRC_FILES :=                                  \
    main.cpp                                        \

LOCAL_SHARED_LIBRARIES :=       	\
    libandroid_runtime          	\
    libbase                     	\
    libcutils                   	\
    libhwbinder                 	\
    libnativehelper             	\
    libutils                    	\
    android.hardware.tests.foo@1.0	\
    android.hardware.tests.bar@1.0	\

LOCAL_MODULE_TAGS := samples

LOCAL_CFLAGS := -O0 -g

include $(BUILD_EXECUTABLE)
