# Makefile

TOP = ../../
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
#==================================================
# Build an IOC support library

# <name>.dbd will be created from <name>Include.dbd
DBD += modbus.dbd
DBD += modbusSupport.dbd

INC += drvModbusAsyn.h

LIBRARY_IOC = modbus

LIB_SRCS += drvModbusAsyn.c
LIB_SRCS += modbusInterpose.c

#===========================

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)

#=============================
# build an ioc application

PROD_IOC = modbusApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS += modbus_registerRecordDeviceDriver.cpp
PROD_SRCS_DEFAULT += modbusMain.cpp
PROD_SRCS_vxWorks += -nil-

PROD_LIBS += modbus
PROD_LIBS += asyn
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
