// DelProtect.cpp
// Filesystem mini-filter driver that protects select files from deletion.

#include <fltKernel.h>
#include <dontuse.h>

#include "DelProtect.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#pragma warning(disable : 26812)  // C26812 prefer scoped enums

PFLT_FILTER gFilterHandle;
ULONG_PTR   OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;

/* ----------------------------------------------------------------------------
 *	Operation Registration
 */

// the callbacks array defines the callbacks for the 
// operations which we are interested in filtering
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      0,
      DelProtectPreCreate,
      nullptr },

	{ IRP_MJ_SET_INFORMATION,
	  0,
	  DelProtectPreSetInformation,
	  nullptr },

    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    DelProtectUnload,                           //  MiniFilterUnload

    DelProtectInstanceSetup,                    //  InstanceSetup
    DelProtectInstanceQueryTeardown,            //  InstanceQueryTeardown
    DelProtectInstanceTeardownStart,            //  InstanceTeardownStart
    DelProtectInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};

NTSTATUS NTAPI
DelProtectInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{
	// this routine is called whenever a new instance is created on a volume,
	// giving us a chance to decide if we need to attach to this volume or not
	// - STATUS_SUCCESS           -> attach
	// - STATUS_FLT_DO_NOT_ATTACH -> do not attach

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectInstanceSetup: Entered\n"));

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
DelProtectInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )

{
	// this is called when an instance is being manually deleted by a
	// call to FltDetachVolume or FilterDetach thereby giving us a chance to fail that detach request.

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectInstanceQueryTeardown: Entered\n"));

    return STATUS_SUCCESS;
}

VOID NTAPI
DelProtectInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
	// this routine is called at the start of instance teardown.

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectInstanceTeardownStart: Entered\n"));
}

VOID NTAPI
DelProtectInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
	// this routine is callled at the end of instance teardown

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectInstanceTeardownComplete: Entered\n"));
}


/* ----------------------------------------------------------------------------
 *	Initialization + Unload Routines
 */

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect!DriverEntry: Entered\n") );

    // register with FltMgr to tell it our callback routines
    status = FltRegisterFilter(
		DriverObject,                        
		&FilterRegistration,
		&gFilterHandle 
	);

    FLT_ASSERT(NT_SUCCESS(status));

    if (NT_SUCCESS(status))
	{
		// start filtering IO
        status = FltStartFiltering(gFilterHandle);

        if (!NT_SUCCESS(status))
		{
            FltUnregisterFilter(gFilterHandle);
        }
    }

    return status;
}

NTSTATUS NTAPI
DelProtectUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect!DelProtectUnload: Entered\n") );

    FltUnregisterFilter(gFilterHandle);

    return STATUS_SUCCESS;
}


/* ----------------------------------------------------------------------------
 *	Minifilter Callback Routines
 *
 *  File delete operations may be performed by the system in one of two ways:
 *  - open the file with the FILE_DELETE_ON_CLOSE option, resulting in the file
 *    being deleted once the last handle to the file is closed
 *  - using the IRP_MJ_SET_INFORMAION operation which is sort of a swiss-army
 *    knife for filesystem IO operations, delete being one of the supported operations
 *
 *  Furthremore, the system is clever, and if one mechanism for deleting the
 *  file fails, it will attempt to delete the file with the other operation.
 *
 *  Therefore, if we really want to protected a file from deletion, we need to
 *  intercept file delete requests that originate from both major function sources.
 *  The two major function callbacks we need to intercept are:
 *  - IRP_MJ_CREATE
 *  - IRP_MJ_SET_INFORMATION
 */

FLT_PREOP_CALLBACK_STATUS NTAPI
DelProtectPreCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_    PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->RequestorMode == KernelMode)
	{
		// if the requestor is kernel mode, pass the request on uninterrupted
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	const auto& params = Data->Iopb->Parameters.Create;

	// determine if the FILE_DELETE_ON_CLOSE flag is set
	// - the low 24 bits of the Options field contain CreateOptions flags
	// - the high 8 bits of the Options field contain CreateDisposition flags
	if (!(params.Options & FILE_DELETE_ON_CLOSE))
	{
		// not a delete operation, just pass through
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

    auto status = FLT_PREOP_SUCCESS_NO_CALLBACK;

    if (!DeletePermitted(PsGetCurrentProcess()))
    {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        status = FLT_PREOP_COMPLETE;

        KdPrint(("Delete operation via IRP_MJ_CREATE blocked"));
    }

    return status;
}

FLT_PREOP_CALLBACK_STATUS NTAPI
DelProtectPreSetInformation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_    PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

    auto& params = Data->Iopb->Parameters.SetFileInformation;

    if (params.FileInformationClass != FileDispositionInformation &&
        params.FileInformationClass != FileDispositionInformationEx
        )
    {
        // note a delete operation via set information
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto info = static_cast<PFILE_DISPOSITION_INFORMATION>(params.InfoBuffer);
    if (!info->DeleteFile)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto status = FLT_PREOP_SUCCESS_NO_CALLBACK;

    // determine the process from which this request originated
    auto process = PsGetThreadProcess(Data->Thread);
    NT_ASSERT(process);

    if (!DeletePermitted(process))
    {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        status = FLT_PREOP_COMPLETE;

        KdPrint(("Delete operation via IRP_MJ_SET_INFORMATION blocked"));
    }

	return status;
}

// DeletePermitted
// Determine if the file delete operation is permitted for the specified process.
bool DeletePermitted(const PEPROCESS process)
{
    // determine if the process for which we want to determine if
    // the delete operation is allowed is the process in whose context
    // we are currently executing
    bool currentProcess = PsGetCurrentProcess() == process;
    
    HANDLE hProcess;
    if (currentProcess)
    {
        hProcess = NtCurrentProcess();
    }
    else
    {
        // obtain a handle to the process from the EPROCESS ptr
        auto status = ObOpenObjectByPointer(
            process,
            OBJ_KERNEL_HANDLE,
            nullptr,
            0,
            nullptr,
            KernelMode,
            &hProcess
        );

        if (!NT_SUCCESS(status))
        {
            // give up?
            return true;
        }
    }

    // arbitrary allocation size
    auto allocSize       = 512;
    bool deletePermitted = true;

    // NOTE: interesting API decision
    // why don't we just allocate the size of a UNICODE_STRING structure here?
    // because the API we use to actually get the image path will place the raw
    // bytes of the string itself directly after the string structure
    auto pProcessName = static_cast<PUNICODE_STRING>(ExAllocatePool(PagedPool, allocSize));
    if (nullptr == pProcessName)
    {
        // give up?
        return true;
    }

    RtlZeroMemory(pProcessName, allocSize);

    // do the image name query
    auto status = ZwQueryInformationProcess(
        hProcess,
        ProcessImageFileName,
        pProcessName,
        (allocSize - sizeof(WCHAR)),
        nullptr
    );

    if (NT_SUCCESS(status))
    {
        // NOTE: this substring search is not robust
        if ((wcsstr(pProcessName->Buffer, L"\\System32\\cmd.exe") != nullptr)
            || (wcsstr(pProcessName->Buffer, L"\\SysWOW64\\cmd.exe") != nullptr))
        {
            deletePermitted = false;
        }
    }

    ExFreePool(pProcessName);

    if (!currentProcess)
    {
        // if we are not currently executing in the context of the process
        // for which we performed the query, need to release the handle
        // to the process that we have obtained (reference counting FTW)
        ZwClose(hProcess);
    }

    return deletePermitted;
}

