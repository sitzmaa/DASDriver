/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

EXTERN_C_START

//
// This is the context that can be placed per queue
// and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

    ULONG PrivateDeviceData;  // just a placeholder

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

NTSTATUS
das1QueueInitialize(
    _In_ WDFDEVICE Device
    );

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL das1EvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP das1EvtIoStop;
EVT_WDF_IO_QUEUE_IO_READ das1EvtDeviceRead;
EVT_WDF_IO_QUEUE_IO_WRITE das1EvtDevieWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL das1EvtIoDeviceControl;
EVT_WDF_INTERRUPT_ISR dasEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC dasEvtInterruptDpc;
EXTERN_C_END
