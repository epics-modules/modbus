# Makefile

TOP = ../..
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
INC += modbusInterpose.h
INC += modbus.h

LIBRARY_IOC = modbus

USR_CFLAGS += -DUSE_TYPED_RSET
USR_CPPFLAGS += -DUSE_TYPED_RSET

LIB_SRCS += drvModbusAsyn.cpp
LIB_SRCS += modbusInterpose.c
LIB_SRCS += testModbusSyncIO.cpp
LIB_LIBS += asyn 
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
LIB_SYS_LIBS_WIN32 += ws2_32

#=============================
# build an ioc application

PROD_IOC += modbusApp
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
modbusApp_SRCS_DEFAULT += modbus_registerRecordDeviceDriver.cpp modbusMain.cpp
modbusApp_SRCS_vxWorks += modbus_registerRecordDeviceDriver.cpp

PROD_IOC += testClient
testClient_SRCS += testClient.cpp

PROD_LIBS += modbus
PROD_LIBS += asyn
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_SYS_LIBS_solaris += socket

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
