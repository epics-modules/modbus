/*
 * testModbusSyncIO.cpp
 * 
 * Asyn driver that inherits from the asynPortDriver class to test using pasynInt32SyncIO to Modbus
 * drivers
 *
 * Author: Mark Rivers
 *
 * Created May 27, 2013
 */

#include <stdio.h>

#include <iocsh.h>
#include <epicsThread.h>
#include <asynInt32SyncIO.h>
#include <asynPortDriver.h>

#include <epicsExport.h>

#define TIMEOUT 1.0

static const char *driverName="testModbusSyncIO";


/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */
#define P_SyncIOString  "SYNC_IO"  /* asynInt32,    r/w */
#define P_LockIOString  "LOCK_IO"  /* asynInt32,    r/w */

/** Class that tests using pasynInt32SyncIO to Modbus drivers */
class epicsShareClass testModbusSyncIO : public asynPortDriver {
public:
    testModbusSyncIO(const char *portName, const char *inputDriver, const char* outputDriver);
                 
    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

protected:
    /** Values used for pasynUser->reason, and indexes into the parameter library. */
    int P_SyncIO_;
    #define FIRST_COMMAND P_SyncIO_
    int P_LockIO_;
    #define LAST_COMMAND P_LockIO_
 
private:
    /* Our data */
    asynUser  *pasynUserSyncInput_;
    asynUser  *pasynUserSyncOutput_;
    asynUser  *pasynUserLockForceRead_;
    asynUser  *pasynUserLockInput_;
    asynUser  *pasynUserLockOutput_;
    asynInt32 *pasynInt32Input_;
    void      *pasynInt32InputPvt_;
    asynInt32 *pasynInt32Output_;
    void      *pasynInt32OutputPvt_;
};

#define NUM_PARAMS (&LAST_COMMAND - &FIRST_COMMAND + 1)


/** Constructor for the testModbusSyncIO class.
  * Calls constructor for the asynPortDriver base class. */
testModbusSyncIO::testModbusSyncIO(const char *portName, const char *inputDriver, const char* outputDriver) 
   : asynPortDriver(portName, 
                    1, /* maxAddr */ 
                    (int)NUM_PARAMS,
                    asynInt32Mask | asynDrvUserMask, /* Interface mask */
                    0, /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver blocks and it is not multi-device*/
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0) /* Default stack size*/    
{
  const char *functionName = "testModbusSyncIO";
  asynInterface *pasynInterface;
  asynDrvUser *pasynDrvUser;
  void *drvPvt;
  asynStatus status;

  // Connect to the Modbus drivers for asynInt32SyncIO
  status = pasynInt32SyncIO->connect(inputDriver, 0, &pasynUserSyncInput_, NULL);
  if (status) {
     printf("%s:%s: Error, unable to connect pasynUserInput_ to Modbus input driver %s\n",
       driverName, functionName, inputDriver);
  }
  status = pasynInt32SyncIO->connect(outputDriver, 0, &pasynUserSyncOutput_, NULL);
  if (status) {
     printf("%s:%s: Error, unable to connect pasynUserOutput_ to Modbus output driver %s\n",
       driverName, functionName, outputDriver);
  }
  
  // Connect to the Modbus drivers for asynInt32 calls with lockPort.  This is a little more complex.
  pasynUserLockForceRead_ = pasynManager->createAsynUser(0, 0);
  status = pasynManager->connectDevice(pasynUserLockForceRead_, inputDriver, 0);
  if (status) {
     printf("%s:%s: Error, unable to connect pasynUserLockForceRead_ to Modbus input driver %s\n",
       driverName, functionName, inputDriver);
  }
  pasynUserLockInput_ = pasynManager->createAsynUser(0, 0);
  status = pasynManager->connectDevice(pasynUserLockInput_, inputDriver, 0);
  if (status) {
     printf("%s:%s: Error, unable to connect pasynUserLockInput_ to Modbus input driver %s\n",
       driverName, functionName, inputDriver);
  }
  pasynUserLockOutput_ = pasynManager->createAsynUser(0, 0);
  status = pasynManager->connectDevice(pasynUserLockOutput_, outputDriver, 0);
  if (status) {
     printf("%s:%s: Error, unable to connect pasynUserLockOutput_ to Modbus output driver %s\n",
       driverName, functionName, outputDriver);
  }

  // Get asynInt32 interface in input and output drivers
  pasynInterface = pasynManager->findInterface(pasynUserLockInput_, asynInt32Type, 1);
  if (!pasynInterface) {
      printf("%s:%s: unable to findInterface asynInt32Type for Modbus input driver %s\n",
        driverName, functionName, inputDriver);
  }
  pasynInt32Input_ = (asynInt32 *)pasynInterface->pinterface;
  pasynInt32InputPvt_ = pasynInterface->drvPvt;
  pasynInterface = pasynManager->findInterface(pasynUserLockOutput_, asynInt32Type, 1);
  if (!pasynInterface) {
      printf("%s:%s: unable to findInterface asynInt32Type for Modbus output driver %s\n",
        driverName, functionName, outputDriver);
  }
  pasynInt32Output_ = (asynInt32 *)pasynInterface->pinterface;
  pasynInt32OutputPvt_ = pasynInterface->drvPvt;

  // call drvUserCreate
  pasynInterface = pasynManager->findInterface(pasynUserLockForceRead_, asynDrvUserType, 1);
  if (!pasynInterface) {
      printf("%s:%s: unable to findInterface asynDrvUser for Modbus input driver %s\n",
        driverName, functionName, inputDriver);
  }
  pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
  drvPvt = pasynInterface->drvPvt;
  status = pasynDrvUser->create(drvPvt, pasynUserLockInput_, "MODBUS_DATA", 0, 0);
  if(status) {
    printf("%s:%s: error calling drvUser->create %s\n",
      driverName, functionName, pasynUserLockInput_->errorMessage);
  }
  status = pasynDrvUser->create(drvPvt, pasynUserLockForceRead_, "MODBUS_READ", 0, 0);
  if(status) {
    printf("%s:%s: error calling drvUser->create %s\n",
      driverName, functionName, pasynUserLockForceRead_->errorMessage);
  }
  pasynInterface = pasynManager->findInterface(pasynUserLockOutput_, asynDrvUserType, 1);
  if (!pasynInterface) {
      printf("%s:%s: unable to findInterface asynDrvUser for Modbus output driver %s\n",
        driverName, functionName, outputDriver);
  }
  pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
  drvPvt = pasynInterface->drvPvt;
  status = pasynDrvUser->create(drvPvt, pasynUserLockOutput_, "MODBUS_DATA", 0, 0);
  if(status) {
    printf("%s:%s: error calling drvUser->create %s\n",
      driverName, functionName, pasynUserLockInput_->errorMessage);
  }

  createParam(P_SyncIOString,  asynParamInt32, &P_SyncIO_);
  createParam(P_LockIOString,  asynParamInt32, &P_LockIO_);
}


/** Called when asyn clients call pasynInt32->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus testModbusSyncIO::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int function = pasynUser->reason;
  int status = asynSuccess;
  epicsInt32 inValue;
  const char* functionName = "writeInt32";

  /* Set the parameter in the parameter library. */
  setIntegerParam(function, value);

  if (function == P_SyncIO_) {
    /* Write the data to the Modbus driver */
    status = pasynInt32SyncIO->write(pasynUserSyncOutput_, value, TIMEOUT);
  }
  else if (function == P_LockIO_) {
    /* Do an atomic read/wait/modify/write cycle */
    /* Lock the output port, which we need to do because we are call write() directly */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: locking the Modbus output port\n",
      driverName, functionName);
    status |= pasynManager->lockPort(pasynUserLockOutput_);
    /* Lock the input port, which disables the poller */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: locking the Modbus input port\n",
      driverName, functionName);
    status |= pasynManager->lockPort(pasynUserLockInput_);
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: calling epicsThreadSleep(1.0)\n",
      driverName, functionName);
    epicsThreadSleep(1.0);
    /* Force a read of the input driver */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: forcing a read by the Modbus input driver\n",
      driverName, functionName);
    status |= pasynInt32Input_->write(pasynInt32InputPvt_, pasynUserLockForceRead_, 1);
    /* Read the input value */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: reading the input value from the Modbus input port, incrementing\n",
      driverName, functionName);
    status |= pasynInt32Input_->read(pasynInt32InputPvt_, pasynUserLockInput_, &inValue);
    /* Add the value passed to this function to the current value */
    inValue += value;
    /* Sleep for 2 seconds so we can prove the poller was idle */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: calling epicsThreadSleep(2.0)\n",
      driverName, functionName);
    epicsThreadSleep(2.0);
    /* Write the new value to the output driver */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: writing value of Modbus output port\n",
      driverName, functionName);
    status |= pasynInt32Output_->write(pasynInt32OutputPvt_, pasynUserLockOutput_, inValue);
    /* Unlock the input port */
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: unlocking the Modbus input port\n",
      driverName, functionName);
    status |= pasynManager->unlockPort(pasynUserLockInput_);
    asynPrint(pasynUserLockInput_, ASYN_TRACE_FLOW,
      "%s:%s: unlocking the Modbus output port\n",
      driverName, functionName);
    status |= pasynManager->unlockPort(pasynUserLockOutput_);
    if (status) {
      asynPrint(pasynUserLockInput_, ASYN_TRACE_ERROR,
        "%s:%s: error in one or more calls in sequence = %d\n",
        driverName, functionName, status);
    }
  } else {
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
      "%s:%s: Unknown parameter function=%d, value=%d",
      driverName, functionName, function, value);
    status = asynError;
  }

  /* Do callbacks so higher layers see any changes */
  callParamCallbacks();

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
    "%s:%s: function=%d, value=%d\n", 
    driverName, functionName, function, value);
  return (asynStatus)status;
}

/** Called when asyn clients call pasynInt32->read().
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to read. */
asynStatus testModbusSyncIO::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  int function = pasynUser->reason;
  asynStatus status;
  const char* functionName = "readInt32";

  if ((function == P_SyncIO_) || (function == P_LockIO_)) {
    /* Read the data from the Modbus driver */
    status = pasynInt32SyncIO->read(pasynUserSyncInput_, value, TIMEOUT);
  } 
  else {
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
      "%s:%s: Unknown parameter function=%d, value=%d\n", 
      driverName, functionName, function, *value);
    status = asynError;
  }

  /* Do callbacks so higher layers see any changes */
  callParamCallbacks();

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
    "%s:%s: function=%d, value=%d\n", 
    driverName, functionName, function, *value);
  return status;
}


/* Configuration routine.  Called directly, or from the iocsh function below */

extern "C" {

/** EPICS iocsh callable function to call constructor for the testModbusSyncIO class. */
int testModbusSyncIOConfigure(const char *portName, const char *inputDriver, const char* outputDriver)
{
    new testModbusSyncIO(portName, inputDriver, outputDriver);
    return(asynSuccess);
}


/* EPICS iocsh shell commands */

static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "Modbus input port",iocshArgString};
static const iocshArg initArg2 = { "Modbus output port",iocshArgString};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2};
static const iocshFuncDef initFuncDef = {"testModbusSyncIOConfigure",3,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    testModbusSyncIOConfigure(args[0].sval, args[1].sval, args[2].sval);
}

void testModbusSyncIORegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

epicsExportRegistrar(testModbusSyncIORegister);

}

