# Project Name
TARGET = field_sample

# Sources
CPP_SOURCES = field_sample.cpp 

# Library Locations
LIBDAISY_DIR = /home/pete/Developer/Daisy/libDaisy
DAISYSP_DIR =  /home/pete/Developer/Daisy/DaisySP

# Includes FatFS source files within project.
USE_FATFS = 1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

