# Makefile

TOP = ../..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
#==================================================
# Build an IOC support library

# <name>.dbd will be created from <name>Include.dbd
DBD += modbusApp.dbd
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
modbusApp_DBD += base.dbd
modbusApp_DBD += asyn.dbd drvAsynIPPort.dbd drvAsynSerialPort.dbd
modbusApp_DBD += modbusSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
modbusApp_SRCS_DEFAULT += modbusApp_registerRecordDeviceDriver.cpp modbusMain.cpp
modbusApp_SRCS_vxWorks += modbusApp_registerRecordDeviceDriver.cpp

PROD_IOC += testClient
testClient_SRCS += testClient.cpp

PROD_LIBS += modbus
PROD_LIBS += asyn

ifdef CALC
modbusApp_DBD += calc.dbd calcSupport.dbd
PROD_LIBS += calc
endif

ifdef AUTOSAVE
modbusApp_DBD += asSupport.dbd
PROD_LIBS += autosave
endif

ifdef DEVIOCSTATS
modbusApp_DBD += iocAdmin.dbd
PROD_LIBS += devIocStats
endif

ifdef BUSY
modbusApp_DBD += busyRecord.dbd busySupport.dbd
PROD_LIBS += busy
endif

ifdef SSCAN
  modbusApp_DBD += sscanSupport.dbd
  PROD_LIBS += sscan
endif

PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_SYS_LIBS_solaris += socket

#
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
