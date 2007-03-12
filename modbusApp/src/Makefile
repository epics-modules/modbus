# Makefile

TOP = ../../
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
#==================================================
# Build an IOC support library

# <name>.dbd will be created from <name>Include.dbd
DBD += modbusTCP.dbd

LIBRARY_IOC = modbusTCPLib

LIB_SRCS += drvModbusTCPAsyn.c

#===========================

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)

#=============================
# build an ioc application

PROD_IOC = modbusTCPApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS += modbusTCP_registerRecordDeviceDriver.cpp
PROD_SRCS_DEFAULT += modbusTCPMain.cpp
PROD_SRCS_vxWorks += -nil-

PROD_LIBS += modbusTCPLib
PROD_LIBS += asyn
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
