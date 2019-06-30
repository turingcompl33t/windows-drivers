#define POOL_NX_OPTIN 1
#include "ntddk.h"

#define __MODULE__  "VerifierTest"

#define DPF(x)      DbgPrint x

VOID
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("%s : Unloaded\n", __MODULE__ );
}



NTSTATUS
DriverEntry(
    PDRIVER_OBJECT   DriverObject,
    PUNICODE_STRING  RegistryPath)
{
    //KIRQL OldIrql;
    PVOID  p = NULL;

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload = DriverUnload;
    
    if ( ( p = ExAllocatePoolWithTag ( NonPagedPool, 1, 'feeb') ) == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DbgPrint( "%s : Pool=%p \n", __MODULE__, p );

    //KeRaiseIrql ( DISPATCH_LEVEL, &OldIrql );

    if ( p == NULL ) {
        ExFreePool(p);
    }

    return STATUS_SUCCESS;
}

