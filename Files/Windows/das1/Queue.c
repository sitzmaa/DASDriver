/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"
#include "wdasio.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, das1QueueInitialize)
#endif


NTSTATUS
das1QueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchSequential
        );
    
    queueConfig.EvtIoDeviceControl = das1EvtIoDeviceControl;
    queueConfig.EvtIoStop = das1EvtIoStop;
    queueConfig.EvtIoRead = das1EvtDeviceRead;
    queueConfig.EvtIoWrite = das1EvtDevieWrite;



    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID
das1EvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d", 
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);

    NTSTATUS status;
    WDFMEMORY user_memory;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DeviceGetContext(device);
    int clock_command,
        holder;
    PUCHAR address,
        clock_control_reg,
        clock_reg;
    UCHAR sample_cmd = 0,
        clock_command_low,
        clock_command_high,
        stat_reg,
        clock_control;
    WdfRequestSetInformation(Request, OutputBufferLength);
    // Main Function
    // Check IOCTL value
    // Switch Statemnt
    DbgPrint("EnteringIOCTL\n");
    switch (IoControlCode) {
    case IOCTL_DAS_START_SAMPLING:
        // Send start sampling command
        sample_cmd = 24|context->channel;
        context->samp = 1;
        context->DasBufferPointer = 0;
        address = context->BADR2 +DAS_CONTROL_REGISTER;
        //write sample command to hardware
        WRITE_PORT_UCHAR(
            address,//PUCHAR port
            sample_cmd
        );
        break;
    case IOCTL_DAS_STOP_SAMPLING:
        context->samp = 0;
        sample_cmd = 8 | context->channel;
        address = context->BADR2 + DAS_CONTROL_REGISTER;
        //write sample command to hw
        WRITE_PORT_UCHAR(
            address,//PUCHAR port
            sample_cmd
        );
        // Send Stop sampling command
        break;
    case IOCTL_DAS_GET_CHANNEL:
        // Return the current channel of sampling
        address = context->BADR2 +DAS_CONTROL_REGISTER;
        //read from bus
        stat_reg = READ_PORT_UCHAR(
            address
        );
        //bit stuff to get channel
        stat_reg = stat_reg&7;
        //context->outputBuffer = (ULONG)stat_reg;
        DbgPrint("in get channel %d\n", stat_reg);
        context->Register = stat_reg;
        //write to user space
        //SIZE_T length;
        //PVOID userSpace;
        //status = WdfRequestRetrieveInputBuffer(Request, 0, &userSpace, &length);
        status = WdfRequestRetrieveOutputMemory(Request, &user_memory);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        //ProbeForRead(context->outputBuffer, length, 0);
        DbgPrint("found mem\n");
        status = WdfMemoryCopyFromBuffer(user_memory, 0, &context->Register, OutputBufferLength);
        //RtlCopyMemory(userSpace, &context->Register, length);
        //RtlCopyMemory(&context->Register, context->outputBuffer, length);
        //DbgPrint("memoint: %ld\n", context->outputBuffer);
        DbgPrint("wrote: %d\n", context->Register);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }

        DbgPrint("wrote to user\n");
        break;
    case IOCTL_DAS_SET_CHANNEL:
        // set the current channel of sampling
        address = context->BADR2 + DAS_CONTROL_REGISTER;
        stat_reg = READ_PORT_UCHAR(address);
        status = WdfRequestRetrieveInputMemory(Request, &user_memory);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        status = WdfMemoryCopyToBuffer(user_memory, 0, &context->channel, sizeof(char));
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        //stat_reg = (stat_reg & ((UCHAR)~7)) | context->channel;
        if (context->samp == 1) {
            stat_reg = 24 | context->channel;
        }
        else {
            stat_reg = 8 | context->channel;
        }
        WRITE_PORT_UCHAR(
            address,
            stat_reg
        );
        break;
    case IOCTL_DAS_GET_RATE:
        // return the current rate of the clock
        // for a4 this will simply return 1588
        status = WdfRequestRetrieveOutputMemory(Request, &user_memory);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        status = WdfMemoryCopyFromBuffer(user_memory, 0, &context->rate, sizeof(int));
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        break;
    case IOCTL_DAS_SET_RATE:
        // Set the clock rate
        // maximum 1588
        holder = context->rate;
        status = WdfRequestRetrieveInputMemory(Request, &user_memory);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        status = WdfMemoryCopyToBuffer(user_memory, 0, &context->rate, sizeof(int));
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        if (context->rate > 1588) {
            context->rate = holder;
        }
        else
        {
            // Do some magic here
            clock_command = context->rate;
            clock_command = (clock_command * DAS_CLOCK_SPEED);
            clock_command_high = (UCHAR)(clock_command >> 8);
            clock_command_low = (UCHAR)(clock_command & 0xff);
            clock_control = DAS_CLOCK_INITIALIZE_CONTROL_WORD;
            clock_control_reg = context->BADR2 + DAS_CLOCK_CONTROL_REGISTER;
            clock_reg = context->BADR2 + DAS_CLOCK_REGISTER;
            WRITE_PORT_UCHAR(clock_control_reg, clock_control);
            WRITE_PORT_UCHAR(clock_reg, clock_command_low);
            WRITE_PORT_UCHAR(clock_reg, clock_command_high);
        }
        break;

    // Developer IOCTL CMDS
    case IOCTL_DAS_GET_REGISTER:
        break;
    case IOCTL_DAS_SET_REGISTER:
        break;
    default:
        KdPrint(("Invalid IOCTL\n"));
        return;
    }
    WdfRequestComplete(Request, STATUS_SUCCESS);

    return;
}

VOID
das1EvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.
    //

    return;
}

VOID 
das1EvtDeviceRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length) 
/*++
    Routine Description:
        Handles WDF read requests

    Arguments:

        Queue -  Handle to the framework queue object that is associated with the
                 I/O request.

        Request - Handle to a framework request object.

        Length - size in bytes to be read

    Return value:
        Void

    A4 VERSION OF CODE WILL DIFFER FROM FINAL CODE
    IN A4 DEVICE CONTEXT CONTAINS A BUFFER OF 10 SAMPLES
    THE CALL TO IOCTL START SAMPLE WILL SET THE BUFFER POINTER TO THE FIRST VALUE
    READ WILL ALLOW USER TO READ THE 10 SAMPLES
    ONE AT A TIME OR ALL AT ONCE AS REQUESTED
--*/
{

    if (Length <= 0) {
        WdfRequestComplete(Request, STATUS_SUCCESS);
    }
    NTSTATUS status=0;
    WDFMEMORY user_memory;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DeviceGetContext(device);
    PULONG buffer = context->DasSampleBuffer;
    ULONG space = 0;
    ULONG size = DAS_BUFFER_SIZE;
    ULONG toRead = (ULONG)Length;

    //space = context->DasBufferInterruptPointer > context->DasBufferReaderPointer ?
    //    (context->DasBufferInterruptPointer-context->DasBufferReaderPointer)*sizeof(int) :
    //    ((size-context->DasBufferReaderPointer) + context->DasBufferInterruptPointer)*sizeof(int);

    //toRead = toRead > space ?
    //    space : toRead;

    //if (context->DasBufferInterruptPointer > context->DasBufferReaderPointer) {

    //}
    //else {
    //    ULONG UpperRead, LowerRead;
    //    UpperRead = (size - context->DasBufferReaderPointer) * sizeof(int);
    //    LowerRead = (toRead - UpperRead);
    //}

    if (context->DasBufferInterruptPointer == context->DasBufferReaderPointer) {
        WdfRequestSetInformation(Request, 0);
        WdfRequestComplete(Request, STATUS_SUCCESS);
        return;
    }
    DbgPrint("Int in read: %d\nRead in read: %d\n", context->DasBufferInterruptPointer, context->DasBufferReaderPointer);
        if(context->DasBufferInterruptPointer >= context->DasBufferReaderPointer){
            //something in buffer
            space = context->DasBufferInterruptPointer - context->DasBufferReaderPointer;
            status = WdfRequestRetrieveOutputMemory(Request, &user_memory);
            if(space >= Length/4){
                DbgPrint("Case1\n");
                //can complete read
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }
                status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer+context->DasBufferReaderPointer, Length);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }

                context->DasBufferReaderPointer += (ULONG)Length/4;
                if (context->DasBufferReaderPointer >= size)
                    context->DasBufferReaderPointer = 0;

                WdfRequestSetInformation(Request, Length);
                WdfRequestComplete(Request, STATUS_SUCCESS);
                return;
            }else{
                DbgPrint("case2\n");
                //partial complete read
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }
                status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer+context->DasBufferReaderPointer, space * sizeof(int));
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }
                context->DasBufferReaderPointer += (ULONG)space;
                if (context->DasBufferReaderPointer >= size)
                    context->DasBufferReaderPointer = 0;
                if(context->samp){
                    context->Length = (ULONG)Length - (ULONG)(space*4);
                    context->Request = Request;
                    context->OutputBufferPosition = (ULONG)space;
                    context->readWaiting = TRUE;
                    return; 
                }
                WdfRequestSetInformation(Request, space);
            }
        }
        if(context->DasBufferInterruptPointer < context->DasBufferReaderPointer){
            space = size - (context->DasBufferReaderPointer - context->DasBufferInterruptPointer);
            toRead = toRead/4 < space ? toRead : space*4;
            ULONG upperRead, lowerRead,upper4,lower4;
            status = WdfRequestRetrieveOutputMemory(Request, &user_memory);
            if(toRead/4 > (size - context->DasBufferReaderPointer)){
                
                upperRead = size - context->DasBufferReaderPointer;
                lowerRead = toRead/4 - upperRead;
                upper4 = upperRead *4;
                lower4 = upperRead *4;
                if(space >= Length/4){
                    DbgPrint("case3\n");
                    //can complete read
                    status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer+context->DasBufferReaderPointer, upper4);
                    if (!NT_SUCCESS(status)) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                        return;
                    }
                    status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer, lower4);
                    if (!NT_SUCCESS(status)) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                        return;
                    }
                    context->DasBufferReaderPointer = (ULONG)(lowerRead);
                    if (context->DasBufferReaderPointer >= size)
                        context->DasBufferReaderPointer = 0;

                    WdfRequestSetInformation(Request, Length);
                }else{
                //partial complete read
                    DbgPrint("case4\n");

                    status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer+context->DasBufferReaderPointer, upper4);
                    if (!NT_SUCCESS(status)) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                        return;
                    }
                    status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer, lower4);
                    if (!NT_SUCCESS(status)) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                        return;
                    }
                    context->DasBufferReaderPointer = lowerRead;
                    if (context->DasBufferReaderPointer >= size)
                        context->DasBufferReaderPointer = 0;
                    if(context->samp){
                        context->Length = (ULONG)Length - (ULONG)(space*4);
                        context->Request = Request;
                        context->OutputBufferPosition = (ULONG)space;
                        context->readWaiting = TRUE;
                        return; 

                    }
                    WdfRequestSetInformation(Request, space);
                }

            }
            if(space  >= Length/4){
                //can complete read
                DbgPrint("start case 5\n");
                status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer +context->DasBufferReaderPointer, Length);
                if (!NT_SUCCESS(status)) {
                    DbgPrint("status: %d\n", status);
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }
                context->DasBufferReaderPointer += (ULONG)(Length/4);
                if (context->DasBufferReaderPointer >= size)
                    context->DasBufferReaderPointer = 0;
                DbgPrint("case5\n");
                WdfRequestSetInformation(Request, Length);
            }else{
                //partial complete read
                  DbgPrint("case6\n");

                status = WdfMemoryCopyFromBuffer(user_memory, 0, buffer+context->DasBufferReaderPointer, space* 4 );
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
                    return;
                }
                context->DasBufferReaderPointer += (ULONG)space;
                if (context->DasBufferReaderPointer >= size)
                    context->DasBufferReaderPointer = 0;
                  if(context->samp){
                    context->Length = (ULONG)Length - (ULONG)(space*4);
                    context->Request = Request;
                    context->OutputBufferPosition = space;
                    context->readWaiting = TRUE;
                    return; 

                  }
                WdfRequestSetInformation(Request, space);
                
            }

        }
        DbgPrint("completion in read\n");
    WdfRequestComplete(Request, STATUS_SUCCESS);
    return;
}

VOID
das1EvtDevieWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
/*++
* Write is not implemented by this driver
* DO NOT PROCESS REQUESTS OF THIS TYPE
--*/
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Length);
    WdfRequestComplete(Request, STATUS_SUCCESS);
}


BOOLEAN
dasEvtInterruptIsr(WDFINTERRUPT Interrupt, ULONG MessageID)
/*++
* interrupt method will check if EOC
* then, 
* immediately call the dpc object associated with this device
* Therefore completing its request and exiting interrupt mode
--*/
{
    UNREFERENCED_PARAMETER(MessageID);

    PDEVICE_CONTEXT context = DeviceGetContext(WdfInterruptGetDevice(Interrupt));
    PUCHAR address = context->BADR2 + DAS_CONTROL_REGISTER;
    UCHAR word = READ_PORT_UCHAR(address);
    if ((word & 8) == 8) {
        context->samp == 0 ?
            WRITE_PORT_UCHAR(address, (word&~8)) : WRITE_PORT_UCHAR(address, ((word|16)|8));
        if ((DAS_EOC & word) != DAS_EOC) {
            // if not samping... write back control word w/int off : write back control sample on
            // Read Clock Value
            context->clockValue = READ_PORT_UCHAR(context->BADR2 + DAS_CLOCK_REGISTER);
            // Read Sample high and low
            context->DasSample = READ_PORT_UCHAR(context->BADR2 + DAS_SAMPLE_HIGH_BITS_REGISTER);
            DbgPrint("das first reg: %ld\n", context->DasSample);
            context->DasSample = (context->DasSample>>4)|((READ_PORT_UCHAR(context->BADR2 + DAS_SAMPLE_LOW_BITS_REGISTER)<<4));
            DbgPrint("das: %ld\n", context->DasSample);
            // Start next conversion
            WRITE_PORT_UCHAR(context->BADR2 + DAS_SAMPLE_LOW_BITS_REGISTER, word);
            // Latch Clock
            WRITE_PORT_UCHAR(context->BADR2 + DAS_CLOCK_CONTROL_REGISTER, DAS_CLOCK_LATCH);
            // Call Dpc
            WdfInterruptQueueDpcForIsr(Interrupt);
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

VOID
dasEvtInterruptDpc(WDFINTERRUPT Interrupt, WDFOBJECT AssociatedObject)
/*++
* * DPC will handle actual logic
* - perform operation
* - latch clock and call next conversion
--*/
{
    UNREFERENCED_PARAMETER(AssociatedObject);
    DbgPrint("IN the DPC\n");
    PDEVICE_CONTEXT context = DeviceGetContext(WdfInterruptGetDevice(Interrupt));
    PULONG Buffer = context->DasSampleBuffer;
    int BufferSize = DAS_BUFFER_SIZE;
    // time offset math
    ULONG timeOffset = context->clockValue;
    timeOffset = context->rate - timeOffset;
    timeOffset = timeOffset * 1000;
    timeOffset = timeOffset / DAS_CLOCK_SPEED;
    // sample math
    DbgPrint("int point: %d\n", context->DasBufferInterruptPointer);
    DbgPrint("Read Point: %d\n", context->DasBufferReaderPointer);
    ULONG FullData = timeOffset << 16 | context->DasSample;
    Buffer[context->DasBufferInterruptPointer] = FullData;
    context->DasBufferInterruptPointer++;
    if (context->DasBufferInterruptPointer >= (ULONG)BufferSize)
        context->DasBufferInterruptPointer = 0;
    if (context->DasBufferInterruptPointer == context->DasBufferReaderPointer) {
        context->DasBufferReaderPointer++;
        if (context->DasBufferReaderPointer >= (ULONG)BufferSize)
            context->DasBufferReaderPointer = 0;
    }
    
    // needs read
    if (context->readWaiting == TRUE) {
        // do more
        WDFREQUEST Request = context->Request;
        WDFMEMORY user_memory;
        NTSTATUS status = WdfRequestRetrieveOutputMemory(Request, &user_memory);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtInterruptDpc failed %!STATUS!", status);
            return;
        }

        ULONG ToRead = context->Length;
        int InterruptPointer = context->DasBufferInterruptPointer;
        int ReadPointer = context->DasBufferPointer;

        ULONG Space = InterruptPointer > ReadPointer ?
            (ULONG)(InterruptPointer - ReadPointer-1) : (ULONG)((BufferSize - ReadPointer) + InterruptPointer-1);
        Space = Space * 4;
        ToRead = ToRead > Space ?
            Space : ToRead;
        if (ToRead < (ULONG)(BufferSize - ReadPointer)*sizeof(int)) {
            status = WdfMemoryCopyFromBuffer(user_memory,
                context->OutputBufferPosition,
                Buffer+(ULONG)ReadPointer,
                ToRead);
            ReadPointer += (ToRead/sizeof(int));
            context->OutputBufferPosition += (ToRead/sizeof(int));
            if (ReadPointer >= BufferSize) {
                ReadPointer = 0;
            }
            context->Length -= ToRead;
        }
        else {
            ULONG UpperRead, LowerRead;
            UpperRead = (ULONG)(BufferSize - ReadPointer)*sizeof(int);
            LowerRead = ToRead - UpperRead;
            status = WdfMemoryCopyFromBuffer(user_memory,
                context->OutputBufferPosition,
                Buffer + (UINT16)ReadPointer,
                UpperRead);
            context->OutputBufferPosition += (UpperRead/sizeof(int));
            context->Length -= (UpperRead/sizeof(int));
            ReadPointer = 0;
            status = WdfMemoryCopyFromBuffer(user_memory,
                context->OutputBufferPosition,
                Buffer + (UINT16)ReadPointer,
                LowerRead);
            context->OutputBufferPosition += (LowerRead/sizeof(int));
            context->Length -= (LowerRead / sizeof(int));
            ReadPointer = (LowerRead / sizeof(int));
        }
        if (ReadPointer >= BufferSize)
            ReadPointer = 0;
        context->DasBufferPointer = ReadPointer;
        
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "das1EvtIoDeviceControl failed %!STATUS!", status);
            return;
        }
        context->readWaiting = FALSE;
        DbgPrint("Im completing in the dpc\n");
        if (Request != NULL) {

            WdfRequestSetInformation(Request, context->OutputBufferPosition);
            WdfRequestComplete(Request, STATUS_SUCCESS);
        }
        

    }


}