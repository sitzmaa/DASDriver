/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>

#include "driver.h"
#include "driver.tmh"
#include "wdasio.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, das1EvtDeviceAdd)
#pragma alloc_text (PAGE, das1EvtDriverContextCleanup)
#endif


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");


    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = das1EvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           das1EvtDeviceAdd
                           );

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

NTSTATUS
das1EvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS status;



    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();



    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDeviceD0Entry = das1EvtD0Entry;
    pnpCallbacks.EvtDevicePrepareHardware = das1EvtPrepareHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    status = das1CreateDevice(DeviceInit);



    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

VOID
das1EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

VOID
das1EvtDeviceCreateFile(
    WDFDEVICE Device,
    WDFREQUEST Request,
    WDFFILEOBJECT FileObject)
/*++
* Driver Open Equivilant
--*/
{
    UNREFERENCED_PARAMETER(FileObject);
    PDEVICE_CONTEXT context = DeviceGetContext(Device);
    DbgPrint("attempting to open with: %d\n", context->isOpen);
    if (!context->isOpen) {
        context->isOpen = 1;
        KdPrint(("isOpen: %d\n",context->isOpen));
        PUCHAR controlReg = context->BADR2 + DAS_CONTROL_REGISTER;
        PUCHAR clockReg = context->BADR2 + DAS_CLOCK_CONTROL_REGISTER;
        PUCHAR cnter2 = context->BADR2 + DAS_CLOCK_REGISTER;
        UCHAR clockCMD = DAS_CLOCK_INITIALIZE_CONTROL_WORD;
        WRITE_PORT_UCHAR(clockReg, clockCMD);

        ULONG initialClock = (DAS_DEFAULT_RATE*DAS_CLOCK_SPEED)/1000;
        UCHAR commandLow, commandHigh;
        commandHigh = (UCHAR)(initialClock >> 8);
        commandLow = (UCHAR)(initialClock & 0xff);
        WRITE_PORT_UCHAR(cnter2, commandHigh);
        WRITE_PORT_UCHAR(cnter2, commandLow);
        UCHAR command = READ_PORT_UCHAR(controlReg);
        DbgPrint("before change %d\n", command);
        command = (8 & ~((UCHAR)7));
        command = command | context->channel;
        DbgPrint("after change %d\n", command);
        WRITE_PORT_UCHAR(controlReg, command);

        WdfRequestComplete(Request, STATUS_SUCCESS);
    }
    else {
        WdfRequestComplete(Request, STATUS_OPEN_FAILED);
    }
}

NTSTATUS
das1EvtD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
/*++
* DEVICE ENTRY - EQUIVELANT TO ATTACH
* 
* Initializes Context Values
* Sets up BADR1
--*/
{
    UNREFERENCED_PARAMETER(PreviousState);
    DbgPrint("Entering D0\n");
    PAGED_CODE();
    PDEVICE_CONTEXT DeviceContext = DeviceGetContext(Device);

    //Initial Values
    DeviceContext->DasBufferInterruptPointer = 0;
    DeviceContext->DasBufferReaderPointer = 0;
    DeviceContext->DasSample = 0;

    ULONG Command;
    PULONG Address;
    // Set up badr1 address register 1 and 2
    // Get Register value
    Address = (PULONG)(((PUCHAR)DeviceContext->BADR1) + DAS_BADDR1_SETUP_ADDR1);
    Command = READ_PORT_ULONG((PULONG)Address);
    DbgPrint("read initial %d\n", Command);
    // Modify
    Command = (Command & ~0xff) ^ 0x41;
    DbgPrint("write intial %d\n", Command);
    // WriteBack
    WRITE_PORT_ULONG((PULONG)Address, Command);
    // Get Value
    Address = (PULONG)(((PUCHAR)DeviceContext->BADR1) + DAS_BADDR1_SETUP_ADDR2);
    Command = READ_PORT_ULONG((PULONG)Address);
    DbgPrint("read secondary %d\n", Command);
    // Modify
    Command = (Command & ~0x07) ^ 0x06;
    DbgPrint("write secondary %d\n", Command);
    //Writeback
    WRITE_PORT_ULONG((PULONG)Address, Command);

    return STATUS_SUCCESS;
}

VOID
das1EvtDeviceClose(
    WDFFILEOBJECT FileObject)
/*++
* Device Close
--*/
{
    WDFDEVICE dev = WdfFileObjectGetDevice(FileObject);
    PDEVICE_CONTEXT context = DeviceGetContext(dev);
    context->isOpen = 0;

}

NTSTATUS
das1EvtPrepareHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
)
/*++
* Call to prepare hardware ports
* Called by EVT_DEVICE_PREPARE
* Prepares BADDR1 for usage on successful map
* 
* Params:
* Device -> Reference to DAS device
* ResourcesRaw -> Raw form resource list
* ResourcesTranslated -> Read form resource list
* 
* Returns STATUS_SUCCESS on set up
* Returns STATUS_DEVICE_CONFIGURATION_ERROR on internal error
* 
* Post: DeviceContext->BADR1 and DeviceContext->BADR2 are mapped
* REQUIRED: Success required to use driver -> fail setup otherwise
--*/
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    ULONG i;
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN BADR1_SUCCESS = FALSE;
    BOOLEAN BADR2_SUCCESS = FALSE;
    PDEVICE_CONTEXT DeviceContext = DeviceGetContext(Device);
    
    UNREFERENCED_PARAMETER(ResourcesRaw);

    PAGED_CODE();

    for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (!descriptor)
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        if (descriptor->Type == CmResourceTypePort) {
            DbgPrint("port at: %d\n Length: %d\n", descriptor->u.Port.Start.LowPart, descriptor->u.Port.Length);
            if (descriptor->u.Port.Length > 2 && !BADR1_SUCCESS==TRUE) {
                DeviceContext->BADR1 = (PULONG)descriptor->u.Port.Start.LowPart;
                DbgPrint("%in dbg setup badr1\n");
                BADR1_SUCCESS = TRUE;
            }
            else if (!BADR2_SUCCESS==TRUE){
                DeviceContext->BADR2 = (PUCHAR)descriptor->u.Port.Start.LowPart;
                DbgPrint("%in dbg setup badr2\n");
                BADR2_SUCCESS = TRUE;
            }
        }
        //if (BADR1_SUCCESS==TRUE && BADR2_SUCCESS==TRUE)
        //    i = WdfCmResourceListGetCount(ResourcesTranslated);
    }
    
    if (!(BADR1_SUCCESS==TRUE && BADR2_SUCCESS==TRUE))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    DbgPrint("NO CONFIG ERROR HERE!\n");

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to initialized interrupts");
    }
    return status;

}

