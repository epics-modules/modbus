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
DBD += modbusTCPSupport.dbd

LIBRARY_IOC = modbusTCP

LIB_SRCS += drvModbusTCPAsyn.c
LIB_SRCS += modbusInterpose.c

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

PROD_LIBS += modbusTCP
PROD_LIBS += asyn
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
