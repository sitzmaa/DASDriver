/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD das1EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP das1EvtDriverContextCleanup;
EVT_WDF_FILE_CLOSE das1EvtDeviceClose;
EVT_WDF_DEVICE_FILE_CREATE das1EvtDeviceCreateFile;
EVT_WDF_DEVICE_PREPARE_HARDWARE das1EvtPrepareHardware;
EVT_WDF_DEVICE_D0_ENTRY das1EvtD0Entry;

EXTERN_C_END
