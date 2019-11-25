// DelProtect.cpp
// Filesystem mini-filter driver that protects select files from deletion.

#include <fltKernel.h>
#include <dontuse.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#pragma warning(disable : 26812)  // C26812 prefer scoped enums

PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/* ----------------------------------------------------------------------------
 *	Prototypes
 */

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS NTAPI
DelProtectUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
	);

NTSTATUS NTAPI
DelProtectInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID NTAPI
DelProtectInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID NTAPI
DelProtectInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS NTAPI
DelProtectInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS NTAPI
DelProtectPreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_    PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
	);

FLT_PREOP_CALLBACK_STATUS NTAPI
DelProtectPreSetInformation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_    PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
	);

// hack to get access to ZwQueryInformationProcess
NTSTATUS ZwQueryInformationProcess(
	_In_      HANDLE ProcessHandle,
	_In_      PROCESSINFOCLASS ProcessInformationClass,
	_Out_     PVOID ProcessInformation,
	_In_      ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
	);

EXTERN_C_END

// assign the code sections for specified routines
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DelProtectUnload)
#pragma alloc_text(PAGE, DelProtectInstanceQueryTeardown)
#pragma alloc_text(PAGE, DelProtectInstanceSetup)
#pragma alloc_text(PAGE, DelProtectInstanceTeardownStart)
#pragma alloc_text(PAGE, DelProtectInstanceTeardownComplete)
#endif


/* ----------------------------------------------------------------------------
 *	Operation Registration
 */

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
        status = FltStartFiltering( gFilterHandle );

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

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectPreOperation: Entered\n"));

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

	// this is a delete operation!
	// we want to block delete operations originating from ant cmd.exe process,
	// and, luckily for us, the CreateFile operation is invoked synchronously, (always?)
	// the caller is the process that is attempting the delete operation

	auto allocSize = 512;  // arbitrary size large enough for cmd.exe image path

	// NOTE: interesting API decision
	// why don't we just allocate the size of a UNICODE_STRING structure here?
	// because the API we use to actually get the image path will place the raw
	// bytes of the string itself directly after the string structure
	auto pProcessName = static_cast<PUNICODE_STRING>(ExAllocatePool(PagedPool, allocSize));
	if (nullptr == pProcessName)
	{
		// bad...
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	RtlZeroMemory(pProcessName, allocSize);

	// do the image name query
	auto status = ZwQueryInformationProcess(
		NtCurrentProcess(), 
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
			// match, fail the request
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			return FLT_PREOP_COMPLETE;
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS NTAPI
DelProtectPreSetInformation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_    PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("DelProtect!DelProtectPreOperation: Entered\n"));

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

