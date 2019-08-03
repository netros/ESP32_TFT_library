#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := tft_demo

EXTRA_CFLAGS += --save-temps

CFLAGS += -Wno-error=tautological-compare -Wno-implicit-fallthrough -Wno-implicit-function-declaration

include $(IDF_PATH)/make/project.mk

