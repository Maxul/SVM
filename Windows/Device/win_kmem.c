/*
 * win_kmem.c
 * This file is part of NEO
 *
 * Copyright (C) 2017 - Maxul
 *
 * NEO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * NEO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NEO. If not, see <http://www.gnu.org/licenses/>.

    <2017/2/9> Maxul Lee

 */

#include <ntddk.h> // what we need to tame a driver

// 设备号
// 32768-65535 are reserved for use by customers.
#define FILE_DEVICE_KMEMDRV 0x00008010

// 2048-4095 are reserved for customers.
#define KERNEL_IOCTL_INDEX 0x810

#define IOCTL_KERNEL_MAPPHYSTOLIN \
	CTL_CODE(FILE_DEVICE_KMEMDRV, KERNEL_IOCTL_INDEX, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KERNEL_UNMAPPHYSADDR \
	CTL_CODE(FILE_DEVICE_KMEMDRV, KERNEL_IOCTL_INDEX + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push)
#pragma pack(1)
struct tagPhysStruct
{
    DWORD64 dwPhyskmemSizeInBytes;	// 物理空间大小
    DWORD64 pvPhysAddress;			// 起始地址
    DWORD64 PhysicalMemoryHandle;	// 内核句柄
    DWORD64 pvPhysMemLin;			// 物理地址对应的线性（虚拟）地址
    DWORD64 pvPhysSection;			//
};
#pragma pack(pop)

DRIVER_INITIALIZE DriverEntry;

NTSTATUS WinIoDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

void WinIoUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS MapPhysicalMemoryToLinearSpace(PVOID pPhysAddress,
                                        SIZE_T PhyskmemSizeInBytes,
                                        PVOID *ppPhysMemLin,
                                        HANDLE *pPhysicalMemoryHandle,
                                        PVOID *ppPhysSection);

NTSTATUS UnmapPhysicalMemory(HANDLE PhysicalMemoryHandle,
                             PVOID pPhysMemLin,
                             PVOID PhysSection);

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, WinIoDispatch)
#pragma alloc_text( PAGE, WinIoUnload)
#pragma alloc_text( PAGE, UnmapPhysicalMemory)
#pragma alloc_text( PAGE, MapPhysicalMemoryToLinearSpace)
#endif

#define DRIVER_NAME "vmos"
#define OFFSET 0x10
#define MAGIC "TRUST"
// 10MB内核空间
#define KernelMemSize (10 * 1024 * 1024)

PCHAR pKMem;

static VOID Kmem_Free(VOID)
{
    if (pKMem)
        MmFreeContiguousMemory(pKMem);
}

static VOID Kmem_Malloc(int kmemSize)
{
    // 获取物理地址所对应的虚拟地址，并向其中写入连续内存的基址
    struct tagPhysStruct RamPhys;

    RamPhys.pvPhysAddress = 0x00000000;
    RamPhys.dwPhyskmemSizeInBytes = 0x1000;

    /* open kernel physical memory */
    MapPhysicalMemoryToLinearSpace(
        (ULONG_PTR)RamPhys.pvPhysAddress,
        (SIZE_T)RamPhys.dwPhyskmemSizeInBytes,
        (PVOID *)&RamPhys.pvPhysMemLin,
        (HANDLE *)&RamPhys.PhysicalMemoryHandle,
        (PVOID *)&RamPhys.pvPhysSection);

    if ( memcmp(RamPhys.pvPhysMemLin, MAGIC, sizeof(MAGIC)) == 0 ) {
        KdPrint(("KMEM already exists at 0x%0x",
                 *(INT64 *)(RamPhys.pvPhysMemLin + OFFSET)));

        goto out;
    }

    PHYSICAL_ADDRESS phyHighAddr;
    phyHighAddr.QuadPart = 0xFFFFFFFFFFFFFFFF;	// 最大的可用的物理地址

    // 申请连续内核空间
    pKMem = MmAllocateContiguousMemory(kmemSize, phyHighAddr);

    if (pKMem != NULL) {
        // 返回虚拟地址对应的物理地址
        PHYSICAL_ADDRESS PhysAddress = MmGetPhysicalAddress(pKMem);

        UINT64 phys_addr = PhysAddress.QuadPart;

        KdPrint(("addr: 0x%0x, size: 0x%0x", phys_addr, kmemSize));

        memset(pKMem, 'z', kmemSize);

        /* taint memory in order to inform outside */
        memcpy(RamPhys.pvPhysMemLin, MAGIC, sizeof(MAGIC));
        *(PVOID *)(RamPhys.pvPhysMemLin + OFFSET) = phys_addr;
    }

out:
    /* close kernel physical memory */
    UnmapPhysicalMemory((HANDLE)RamPhys.PhysicalMemoryHandle,
                        (PVOID)RamPhys.pvPhysMemLin,
                        (PVOID)RamPhys.pvPhysSection);
}

// 驱动主入口

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
                     IN PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING  DeviceName;
    UNICODE_STRING  DeviceLink;

    NTSTATUS        ntStatus;
    PDEVICE_OBJECT  DeviceObject = NULL;

    KdPrint(("\nKMEM Driver -- Compiled %s %s\n", __DATE__, __TIME__));

    KdPrint(("KMEM Entering"));

    DriverObject->MajorFunction[IRP_MJ_CREATE] = WinIoDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = WinIoDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = WinIoDispatch;

    DriverObject->DriverUnload = WinIoUnload;

    RtlInitUnicodeString(&DeviceName, L"\\Device\\"DRIVER_NAME);

    ntStatus = IoCreateDevice(
                   DriverObject,
                   0, // 无设备扩展
                   &DeviceName,
                   FILE_DEVICE_KMEMDRV,
                   FILE_DEVICE_SECURE_OPEN,
                   FALSE,
                   &DeviceObject);

    if (!NT_SUCCESS(ntStatus)) {
        KdPrint(("KMEM: IoCreateDevice failed.  Status = 0x%0x\n", ntStatus));
        return ntStatus;
    }

    // 设备链接名，用于用户空间打开
    RtlInitUnicodeString(&DeviceLink, L"\\DosDevices\\"DRIVER_NAME);

    ntStatus = IoCreateSymbolicLink(&DeviceLink, &DeviceName);

    if (!NT_SUCCESS(ntStatus)) {
        IoDeleteDevice(DeviceObject);
        KdPrint(("KMEM: IoCreateSymbolicLink failed.  Status = 0x%0x\n", ntStatus));
        return ntStatus;
    }

    Kmem_Malloc(KernelMemSize); // malloc kmem when first installed

    KdPrint(("KMEM Leaving"));

    return ntStatus;
}


// Process the IRPs sent to this device

NTSTATUS WinIoDispatch(IN PDEVICE_OBJECT DeviceObject,
                       IN PIRP Irp)
{
    PIO_STACK_LOCATION IrpStack;
    ULONG              dwInputBufferLength;
    ULONG              dwOutputBufferLength;
    PVOID              pvIOBuffer;
    NTSTATUS           ntStatus;
    struct             tagPhysStruct PhysStruct;
    struct             tagPhysStruct32 *pPhysStruct32 = NULL;

    // 初始化为默认值

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IrpStack = IoGetCurrentIrpStackLocation(Irp);

    // 获取输入/输出缓冲区地址和长度
    pvIOBuffer = Irp->AssociatedIrp.SystemBuffer;

    dwInputBufferLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
    dwOutputBufferLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Kmem_Malloc(KernelMemSize);
        break;

    case IRP_MJ_CLOSE:
        // Kmem_Free();
        break;

    case IRP_MJ_DEVICE_CONTROL:

        switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_KERNEL_MAPPHYSTOLIN:

            KdPrint((" IOCTL_KERNEL_MAPPHYSTOLIN "));

            if (dwInputBufferLength) {

                memcpy(&PhysStruct, pvIOBuffer, dwInputBufferLength);

                ntStatus = MapPhysicalMemoryToLinearSpace(
                               (PVOID)PhysStruct.pvPhysAddress,
                               (SIZE_T)PhysStruct.dwPhyskmemSizeInBytes,
                               (PVOID *)&PhysStruct.pvPhysMemLin,
                               (HANDLE *)&PhysStruct.PhysicalMemoryHandle,
                               (PVOID *)&PhysStruct.pvPhysSection);

                if (NT_SUCCESS(ntStatus)) {
                    memcpy(pvIOBuffer, &PhysStruct, dwInputBufferLength);
                    Irp->IoStatus.Information = dwInputBufferLength;
                }

                Irp->IoStatus.Status = ntStatus;
            }
            else {
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            }

            break;

        case IOCTL_KERNEL_UNMAPPHYSADDR:

            KdPrint((" IOCTL_KERNEL_UNMAPPHYSADDR "));

            if (dwInputBufferLength) {

                memcpy(&PhysStruct, pvIOBuffer, dwInputBufferLength);

                ntStatus = UnmapPhysicalMemory(
                               (HANDLE)PhysStruct.PhysicalMemoryHandle,
                               (PVOID)PhysStruct.pvPhysMemLin,
                               (PVOID)PhysStruct.pvPhysSection);

                Irp->IoStatus.Status = ntStatus;
            }
            else {
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            }

            break;

        default:

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            break;
        }

        break;
    }

    ntStatus = Irp->IoStatus.Status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}

// Delete the associated device and return

static VOID WinIoUnload(IN PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DeviceLink;

    RtlInitUnicodeString(&DeviceLink, L"\\DosDevices\\"DRIVER_NAME);

    IoDeleteSymbolicLink(&DeviceLink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

// 物理地址映射函数

NTSTATUS MapPhysicalMemoryToLinearSpace(PVOID pPhysAddress,
                                        SIZE_T PhyskmemSizeInBytes,
                                        PVOID *ppPhysMemLin,
                                        HANDLE *pPhysicalMemoryHandle,
                                        PVOID *ppPhysSection)
{
    UNICODE_STRING     PhysicalMemory;
    OBJECT_ATTRIBUTES  ObjectAttributes;
    PHYSICAL_ADDRESS   ViewBase;
    NTSTATUS           ntStatus;
    PHYSICAL_ADDRESS   pStartPhysAddress;
    PHYSICAL_ADDRESS   pEndPhysAddress;
    BOOLEAN            Result1, Result2;
    ULONG              IsIOSpace;
    unsigned char     *pbPhysMemLin = NULL;

    KdPrint(("Entering MapPhysicalMemoryToLinearSpace"));

    RtlInitUnicodeString(&PhysicalMemory, L"\\Device\\PhysicalMemory");

    InitializeObjectAttributes(
        &ObjectAttributes,
        &PhysicalMemory,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        (PSECURITY_DESCRIPTOR)NULL);

    *pPhysicalMemoryHandle = NULL;
    *ppPhysSection = NULL;

    ntStatus = ZwOpenSection(pPhysicalMemoryHandle,
                             SECTION_ALL_ACCESS,
                             &ObjectAttributes);

    if (NT_SUCCESS(ntStatus)) {

        ntStatus = ObReferenceObjectByHandle(*pPhysicalMemoryHandle,
                                             SECTION_ALL_ACCESS,
                                             (POBJECT_TYPE)NULL,
                                             KernelMode,
                                             ppPhysSection,
                                             (POBJECT_HANDLE_INFORMATION)NULL);

        if (NT_SUCCESS(ntStatus)) {
            pStartPhysAddress.QuadPart = (ULONGLONG)(ULONG_PTR)pPhysAddress;

            pEndPhysAddress.QuadPart = pStartPhysAddress.QuadPart + PhyskmemSizeInBytes;

            IsIOSpace = 0;

            Result1 = HalTranslateBusAddress(1, 0,
                                             pStartPhysAddress,
                                             &IsIOSpace,
                                             &pStartPhysAddress);

            IsIOSpace = 0;

            Result2 = HalTranslateBusAddress(1, 0,
                                             pEndPhysAddress,
                                             &IsIOSpace,
                                             &pEndPhysAddress);

            if (Result1 && Result2) {
                // Let ZwMapViewOfSection pick a linear address

                PhyskmemSizeInBytes = (SIZE_T)pEndPhysAddress.QuadPart - (SIZE_T)pStartPhysAddress.QuadPart;

                ViewBase = pStartPhysAddress;

                ntStatus = ZwMapViewOfSection(*pPhysicalMemoryHandle,
                                              (HANDLE)-1,
                                              &pbPhysMemLin,
                                              0L,
                                              PhyskmemSizeInBytes,
                                              &ViewBase,
                                              &PhyskmemSizeInBytes,
                                              ViewShare,
                                              0,
                                              PAGE_READWRITE | PAGE_NOCACHE);

                // If the physical memory is already mapped with a different caching attribute, try again
                if (ntStatus == STATUS_CONFLICTING_ADDRESSES) {
                    ntStatus = ZwMapViewOfSection(*pPhysicalMemoryHandle,
                                                  (HANDLE)-1,
                                                  &pbPhysMemLin,
                                                  0L,
                                                  PhyskmemSizeInBytes,
                                                  &ViewBase,
                                                  &PhyskmemSizeInBytes,
                                                  ViewShare,
                                                  0,
                                                  PAGE_READWRITE);
                }

                if (!NT_SUCCESS(ntStatus))
                    KdPrint(("KMEM: ZwMapViewOfSection failed"));
                else
                {
                    pbPhysMemLin += pStartPhysAddress.QuadPart - ViewBase.QuadPart;
                    *ppPhysMemLin = pbPhysMemLin;
                }
            }
            else
                KdPrint(("KMEM: HalTranslateBusAddress failed"));
        }
        else
            KdPrint(("KMEM: ObReferenceObjectByHandle failed"));
    }
    else
        KdPrint(("KMEM: ZwOpenSection failed"));

    if (!NT_SUCCESS(ntStatus))
        ZwClose(*pPhysicalMemoryHandle);

    KdPrint(("Leaving MapPhysicalMemoryToLinearSpace"));

    return ntStatus;
}


NTSTATUS UnmapPhysicalMemory(HANDLE PhysicalMemoryHandle,
                             PVOID pPhysMemLin,
                             PVOID pPhysSection)
{
    NTSTATUS ntStatus;

    KdPrint(("Entering UnmapPhysicalMemory"));

    ntStatus = ZwUnmapViewOfSection((HANDLE)-1, pPhysMemLin);

    if (!NT_SUCCESS(ntStatus))
        KdPrint(("KMEM: UnmapViewOfSection failed"));

    if (pPhysSection)
        ObDereferenceObject(pPhysSection);

    ZwClose(PhysicalMemoryHandle);

    KdPrint(("Leaving UnmapPhysicalMemory"));

    return ntStatus;
}

