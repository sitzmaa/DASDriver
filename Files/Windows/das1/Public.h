/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_das1,
    0x6a78a67b,0x80f9,0x4bec,0x94,0x61,0xf6,0x25,0x0d,0x76,0x65,0xc9);
// {6a78a67b-80f9-4bec-9461-f6250d7665c9}
