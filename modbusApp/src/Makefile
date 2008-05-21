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
LIB_LIBS += asyn 
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

#Note that the command line that builds the
#library $(LIBNAME) may be HUGE (>3kB)

#=============================
# build an ioc application

PROD_IOC = modbusApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS_DEFAULT += modbus_registerRecordDeviceDriver.cpp modbusMain.cpp
PROD_SRCS_vxWorks += modbus_registerRecordDeviceDriver.cpp

PROD_LIBS += modbus
PROD_LIBS += asyn
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
