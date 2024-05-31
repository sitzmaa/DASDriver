/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, das1CreateDevice)
#endif

NTSTATUS
das1CreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status = 0;

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    WDF_FILEOBJECT_CONFIG deviceConfig;

    WDF_FILEOBJECT_CONFIG_INIT(
        &deviceConfig,
        das1EvtDeviceCreateFile,
        das1EvtDeviceClose,
        WDF_NO_EVENT_CALLBACK // No cleanup callback function
    );
    WdfDeviceInitSetFileObjectConfig(
        DeviceInit,
        &deviceConfig,
        &deviceAttributes
    );


    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        //
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. DeviceGetContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        //
        deviceContext = DeviceGetContext(device);

        //
        // Initialize the context.
        //
        deviceContext->DasBufferPointer = 0;
        deviceContext->DasBufferInterruptPointer = 0;
        deviceContext->OutputBufferPosition = 0;
        deviceContext->isOpen = 0;
        deviceContext->readWaiting = FALSE;
        deviceContext->isrRequest = FALSE;
        deviceContext->rate = DAS_DEFAULT_RATE;
        deviceContext->channel = DAS_DEFAULT_CHANNEL;
        deviceContext->DasSample = 0;
        deviceContext->clockValue = 0;
        deviceContext->Length = 0;
        deviceContext->Request = NULL;
        deviceContext->samp = 0;
        int size = DAS_BUFFER_SIZE;
        for (int i = 0; i < size + 1; i++)
            deviceContext->DasSampleBuffer[i] = 0;



        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_das1,
            NULL // ReferenceString
            );

        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            status = das1QueueInitialize(device);
        }
        WDF_INTERRUPT_CONFIG InterruptConfig;
        WDF_INTERRUPT_CONFIG_INIT(&InterruptConfig, dasEvtInterruptIsr, dasEvtInterruptDpc);
        status = WdfInterruptCreate(device, &InterruptConfig, WDF_NO_OBJECT_ATTRIBUTES, &deviceContext->DasInterrupt);
        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            //UNICODE_STRING link;
            DECLARE_CONST_UNICODE_STRING(link, L"\\??\\das1");
            status = WdfDeviceCreateSymbolicLink(device,&link);
        }
        else {
            DbgPrint("Interupts failed in device.c\n");
        }

    }

    return status;
}
