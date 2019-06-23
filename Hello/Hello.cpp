/*
 * Hello.cpp
 * The "Hello World" of Windows driver programming. 
 */

#include <ntddk.h>

void HelloUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{	
	// we compile with "warnings as errors", so make the compiler happy 
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = HelloUnload;

	KdPrint(("Hello Driver Load!\n"));

	return STATUS_SUCCESS;
}

void HelloUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	KdPrint(("Hello Driver Unload!"));
}