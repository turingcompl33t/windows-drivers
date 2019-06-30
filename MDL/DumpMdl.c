/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2019 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#include "ntifs.h"

#define DPF(x)      DbgPrint x
#define __MODULE__  "Mdls"

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

BOOLEAN
MapMemory(
    PVOID Address,
    ULONG Length,
    PVOID* MappedAddressPtr,
    PMDL* MdlPtr);

VOID UnmapMemory(
    PVOID Address,
    PMDL Mdl);

PUCHAR g_MappedAddress = NULL;
PMDL g_MappedMdl = NULL;
PUCHAR g_Alloc = NULL;

VOID 
DumpMdl(
    PMDL Mdl,
    PCHAR Tag )
{
    PPFN_NUMBER PfnArray;
    ULONG PfnCount;
    ULONG Idx;

    DPF(("Mdl @ %p [%s]\n", Mdl, Tag ));

    DPF(("%20s : %p\n", "Next ", Mdl->Next));
    DPF(("%20s : %u\n", "Size ", Mdl->Size));
    DPF(("%20s : %08x\n", "MdlFlags ", Mdl->MdlFlags));

    if (Mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {
        DPF(("%20s : %s\n", "", "MDL_MAPPED_TO_SYSTEM_VA"));
    }
    if (Mdl->MdlFlags & MDL_PAGES_LOCKED) {
        DPF(("%20s : %s\n", "", "MDL_PAGES_LOCKED"));
    }
    if (Mdl->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL) {
        DPF(("%20s : %s\n", "", "MDL_SOURCE_IS_NONPAGED_POOL"));
    }
    if (Mdl->MdlFlags & MDL_ALLOCATED_FIXED_SIZE) {
        DPF(("%20s : %s\n", "", "MDL_ALLOCATED_FIXED_SIZE"));
    }
    if (Mdl->MdlFlags & MDL_PARTIAL) {
        DPF(("%20s : %s\n", "", "MDL_PARTIAL"));
    }
    if (Mdl->MdlFlags & MDL_PARTIAL_HAS_BEEN_MAPPED) {
        DPF(("%20s : %s\n", "", "MDL_PARTIAL_HAS_BEEN_MAPPED"));
    }
    if (Mdl->MdlFlags & MDL_IO_PAGE_READ) {
        DPF(("%20s : %s\n", "", "MDL_IO_PAGE_READ"));
    }
    if (Mdl->MdlFlags & MDL_WRITE_OPERATION) {
        DPF(("%20s : %s\n", "", "MDL_WRITE_OPERATION"));
    }

    if (Mdl->MdlFlags & MDL_LOCKED_PAGE_TABLES) {
        DPF(("%20s : %s\n", "", "MDL_LOCKED_PAGE_TABLES"));
    }

    if (Mdl->MdlFlags & MDL_FREE_EXTRA_PTES) {
        DPF(("%20s : %s\n", "", "MDL_FREE_EXTRA_PTES"));
    }

    if (Mdl->MdlFlags & MDL_DESCRIBES_AWE) {
        DPF(("%20s : %s\n", "", "MDL_DESCRIBES_AWE"));
    }

    if (Mdl->MdlFlags & MDL_IO_SPACE) {
        DPF(("%20s : %s\n", "", "MDL_IO_SPACE"));
    }

    if (Mdl->MdlFlags & MDL_NETWORK_HEADER) {
        DPF(("%20s : %s\n", "", "MDL_NETWORK_HEADER"));
    }

    if (Mdl->MdlFlags & MDL_MAPPING_CAN_FAIL) {
        DPF(("%20s : %s\n", "", "MDL_MAPPING_CAN_FAIL"));
    }

    if (Mdl->MdlFlags & MDL_PAGE_CONTENTS_INVARIANT) {
        DPF(("%20s : %s\n", "", "MDL_PAGE_CONTENTS_INVARIANT"));
    }

    if (Mdl->MdlFlags & MDL_INTERNAL) {
        DPF(("%20s : %s\n", "", "MDL_INTERNAL"));
    }

    DPF(("%20s : %p\n", "Process", Mdl->Process));
    DPF(("%20s : %p\n", "MappedSystemVa", Mdl->MappedSystemVa));
    DPF(("%20s : %p\n", "StartVa", Mdl->StartVa));
    DPF(("%20s : 0x%x\n", "ByteCount", MmGetMdlByteCount(Mdl) ));
    DPF(("%20s : 0x%x\n", "ByteOffset", MmGetMdlByteOffset(Mdl) ));

    PfnArray = MmGetMdlPfnArray(Mdl);
    PfnCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(Mdl), MmGetMdlByteCount(Mdl));

    DPF(("%20s : %p (%u) :\n", "PfnArray", PfnArray, PfnCount));

    for (Idx = 0; Idx < PfnCount; Idx++) {
        DPF(("%20s : [%u] %p\n", "", Idx, PfnArray[Idx]));
    }
}



NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    PUCHAR Address;
    ULONG Size;
    ULONG Extra;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    DriverObject->DriverUnload = DriverUnload;

    Extra = 16;
    Size = (2 * 1024) + Extra;

    g_Alloc = ExAllocatePoolWithTag(PagedPool, Size, 'sldM');
    if (!g_Alloc) {
        Status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    Address = g_Alloc + Extra;

    DbgPrint("%s Address=%p Alloc=%p Size=%x\n", __FUNCTION__, Address, g_Alloc, Size);

    if (!MapMemory(
        Address,
        Size,
        &g_MappedAddress,
        &g_MappedMdl)) {

        Status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    DbgPrint("%s MappedAddress=%p MappedMdl=%p\n", __FUNCTION__,
        g_MappedAddress,
        g_MappedMdl);

    Status = STATUS_SUCCESS;

Exit:
    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    UnmapMemory(g_MappedAddress, g_MappedMdl);

    if (g_Alloc) {
        ExFreePool(g_Alloc);
    }

} // DriverUnload()

BOOLEAN
MapMemory(
    PUCHAR Address,
    ULONG Length,
    PVOID* MappedAddressPtr,
    PMDL* MdlPtr)
{
    PMDL Mdl = NULL;
    PUCHAR MappedAddress = NULL;

    Mdl = IoAllocateMdl(Address, Length, FALSE, FALSE, NULL);
    if (Mdl == NULL) {
        goto Exit;
    }
    
    DumpMdl(Mdl, "IoAllocateMdl");

    __try {
        MmProbeAndLockPages(Mdl, KernelMode, IoModifyAccess);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        goto Exit;
    }

    DumpMdl(Mdl, "MmProbeAndLockPages");

    MappedAddress = MmMapLockedPagesSpecifyCache(Mdl, KernelMode, MmCached, NULL, FALSE, HighPagePriority | MdlMappingNoExecute);
    if (MappedAddress == NULL) {
        goto Exit;
    }

    DumpMdl(Mdl, "MmMapLockedPagesSpecifyCache");

    *MappedAddressPtr = MappedAddress;
    *MdlPtr = Mdl;

    return TRUE;

Exit:
    if (MappedAddress) {
        MmUnmapLockedPages(MappedAddress, Mdl);
    }

    if (Mdl) {
        MmUnlockPages(Mdl);
    }

    if (Mdl) {
        IoFreeMdl(Mdl);
    }

    return FALSE;
} // MapMemory()

VOID UnmapMemory(
    PUCHAR Address,
    PMDL Mdl)
{
    MmUnmapLockedPages(Address, Mdl);

    DumpMdl(Mdl, "MmUnmapLockedPages");

    MmUnlockPages(Mdl);

    DumpMdl(Mdl, "MmUnlockPages");

    IoFreeMdl(Mdl);

} // UnmapMemory()
