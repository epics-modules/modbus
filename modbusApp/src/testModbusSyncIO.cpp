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
#include <asynInt32SyncIO.h>
#include <asynPortDriver.h>

#include <epicsExport.h>

#define TIMEOUT 1.0

static const char *driverName="testModbusSyncIO";


/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */
#define P_DataString   "DATA"  /* asynInt32,    r/o */

/** Class that tests using pasynInt32SyncIO to Modbus drivers */
class testModbusSyncIO : public asynPortDriver {
public:
    testModbusSyncIO(const char *portName, const char *inputDriver, const char* outputDriver);
                 
    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

protected:
    /** Values used for pasynUser->reason, and indexes into the parameter library. */
    int P_Data_;
    #define FIRST_COMMAND P_Data_
    #define LAST_COMMAND P_Data_
 
private:
    /* Our data */
    asynUser *pasynUserInput_;
    asynUser *pasynUserOutput_;
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
  asynStatus status;

  // Connect to the Modbus drivers
  status = pasynInt32SyncIO->connect(inputDriver, 0, &pasynUserInput_, NULL);
  if (status) {
     printf("%s:%s: Error, unable to connect to Modbus input driver %s\n",
       driverName, functionName, inputDriver);
  }
  status = pasynInt32SyncIO->connect(outputDriver, 0, &pasynUserOutput_, NULL);
  if (status) {
     printf("%s:%s: Error, unable to connect to Modbus output driver %s\n",
       driverName, functionName, outputDriver);
  }

  createParam(P_DataString,  asynParamInt32, &P_Data_);
}


/** Called when asyn clients call pasynInt32->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus testModbusSyncIO::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int function = pasynUser->reason;
  asynStatus status;
  const char* functionName = "writeInt32";

  /* Set the parameter in the parameter library. */
  setIntegerParam(function, value);

  if (function == P_Data_) {
    /* Write the data to the Modbus driver */
    status = pasynInt32SyncIO->write(pasynUserOutput_, value, TIMEOUT);
  } 
  else {
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
  return status;
}

/** Called when asyn clients call pasynInt32->read().
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to read. */
asynStatus testModbusSyncIO::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  int function = pasynUser->reason;
  asynStatus status;
  const char* functionName = "readInt32";

  if (function == P_Data_) {
    /* Read the data from the Modbus driver */
    status = pasynInt32SyncIO->read(pasynUserInput_, value, TIMEOUT);
  } 
  else {
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
      "%s:%s: Unknown parameter function=%d, value=%d", 
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

