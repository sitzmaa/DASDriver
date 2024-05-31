/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"
#include "wdm.h"
#include "wdasio.h"
EXTERN_C_START

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
    ULONG DasSampleBuffer[2000];
    ULONG DasBufferPointer;
    ULONG clockValue,
        Length,
        rate;
    BOOLEAN isOpen;
    BOOLEAN readWaiting;
    BOOLEAN isrRequest;
    PULONG BADR1;
    PUCHAR BADR2;
    UCHAR samp,
        channel;
    ULONG DasBufferInterruptPointer,
        DasBufferReaderPointer,
        OutputBufferPosition;
    WDFREQUEST Request;
    UINT16 DasSample;
    WDFINTERRUPT DasInterrupt;
    ULONG Register;
    PUINT32 outputBuffer;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
das1CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EXTERN_C_END
