#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

#include "winio.h"

#define MEMSIZE (5 * 1024 * 1024)
#define MAGIC "TRUST"
#define OFFSET 0x10

PVOID ram;
PVOID mem;
UINT64 offset;

PCHAR _stdcall open_mem(tagPhysStruct *PhysStruct)
{
    return MapPhysToLin(PhysStruct);
}

BOOL _stdcall close_mem(tagPhysStruct *PhysStruct)
{
    return UnmapPhysicalMemory(PhysStruct);
}

DWORD _stdcall get_offset(VOID)
{
    DWORD PhysAddr_Kmem;

    if (!IsWinIoInitialized)
        return -1;

    GetPhysLong((PBYTE)OFFSET, &PhysAddr_Kmem);

    return PhysAddr_Kmem;
}

VOID __cdecl main(_In_ ULONG argc, _In_reads_(argc) PCHAR argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    //ShutdownWinIo();
    InitializeWinIo();

    if (!IsWinIoInitialized) {
        for (;;) {
            DWORD option;
            DWORD PhysAddr64;
            DWORD Length;

            scanf_s("%x", &PhysAddr64, sizeof(PhysAddr64));
            scanf_s("%x", &Length, sizeof(Length));

            if (Length < 2) {
                ShutdownWinIo();
                return;
            }

            scanf_s("%d", &option, sizeof(option));
            if (1 == option)
                GetContiguousPhysLong(PhysAddr64, &Length);
            else
                SetContiguousPhysLong(PhysAddr64, (DWORD)0xff, &Length);
        }
    }

    tagPhysStruct Phys;

    Phys.pvPhysAddress = (DWORD64)(DWORD32)0x0000;
    Phys.dwPhysMemSizeInBytes = 0x1000;
    ram = open_mem(&Phys);

    offset = get_offset();
    if (-1 == offset) {
        fprintf(stderr, "get offset failed\n");
        return;
    }

    Phys.pvPhysAddress = offset;
    Phys.dwPhysMemSizeInBytes = MEMSIZE;
    mem = open_mem(&Phys);

    memset(mem, 0, MEMSIZE);

}

