include $(THEOS)/makefiles/common.mk

DEBUG = 0
FOR_RELEASE = 1
FINALPACKAGE = 1
#ARCHS = armv7
ARCHS = arm64
#ARCHS = armv7 arm64
TOOL_NAME = rxmemscan
TARGET_CODESIGN = /usr/local/Cellar/ldid/1.2.1/bin/ldid
TARGET_CODESIGN_FLAGS = -Sent.xml
ADDITIONAL_CFLAGS += -Imissing_headers -include Prefix.pch -Wno-c++11-extensions
ADDITIONAL_LDFLAGS += -lreadline -lncurses

rxmemscan_FILES = main.cpp lz4/lz4.c rx_mem_scan.cpp
include $(THEOS_MAKE_PATH)/tool.mk

