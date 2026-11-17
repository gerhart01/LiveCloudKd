#pragma once

#ifndef __HVMM_MAPPED_MEMORY_H__
#define __HVMM_MAPPED_MEMORY_H__

//
// HvmmMappedMemory: read guest physical memory through a persistent, read-only
// user-mode mapping created by hvmm.sys (IOCTL_MAP_GPA_RANGE), instead of one
// kernel round trip per page through SdkReadPhysicalMemory.
//
// hvlib.dll still owns partition enumeration, KDBG scanning and VA translation.
// This helper only needs three things from it: the vid partition handle
// (InfoPartitionHandle), the memory run table (InfoNumberOfRuns / InfoRun) and
// SdkReadPhysicalMemory as the fallback for anything that is not mapped.
//
// Only full VMs are backed by host pages that hvmm.sys can map. For containers
// the map call fails and every read silently falls back, so callers never lose
// coverage, only speed.
//
// A mapping outlives the VM. Nothing pins the guest's host pages: once the VM
// stops, the host reuses them and the mapping shows whatever lands there. Reads
// keep succeeding and return wrong bytes. Callers must watch the VM themselves
// (HvmmMappedIsAlive, or SdkEnumPartitions) and close the context when it is
// gone. HvmmMappedReadPhysicalMemory may run on many threads; HvmmMappedClose
// waits for the reads in flight through it. Direct HvmmMappedRead and
// HvmmMappedGetPointer callers must not close the context under their own reads.
//

#include <windows.h>
#include <winioctl.h>
#include "HvlibEnumPublic.h"
#include "HvlibHandle.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Wire format. Keep in sync with hvmm/hvmm/hvmm.h.
//
// Other hvmm.sys builds use the codes right after 0x834 for their own requests, so the mapping
// codes start at 0x8A0, and nothing but the query is sent until the driver has answered it with
// HVMM_MAPPING_SIGNATURE (HvmmQueryMappingSupport).
//

#define HVMM_IOCTL_QUERY_MAPPING_SUPPORT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8A0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HVMM_IOCTL_MAP_GPA_RANGE         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8A1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HVMM_IOCTL_UNMAP_GPA_RANGE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8A2, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HVMM_MAPPING_QUERY_MAGIC   (0x714D7648UL)   // "HvMq"
#define HVMM_MAPPING_SIGNATURE     (0x704D7648UL)   // "HvMp"
#define HVMM_MAPPING_VERSION       (1UL)

//
// The driver reports the largest single map request it accepts in the handshake
// (MaxMapLength); the helper chunks by that value. This constant is only the
// smallest answer the helper accepts.
//

#define HVMM_MAP_GPA_MIN_MAX_LENGTH (0x1000ULL)
#define HVMM_DEVICE_PATH           L"\\\\.\\hvmm"

typedef struct _HVMM_MAPPING_QUERY_INPUT {
	UINT32 Magic;           // HVMM_MAPPING_QUERY_MAGIC
	UINT32 Reserved;
} HVMM_MAPPING_QUERY_INPUT, *PHVMM_MAPPING_QUERY_INPUT;

typedef struct _HVMM_MAPPING_QUERY_OUTPUT {
	UINT32 Signature;       // HVMM_MAPPING_SIGNATURE
	UINT32 Version;         // HVMM_MAPPING_VERSION
	UINT64 MaxMapLength;    // longest range this driver maps in one call
} HVMM_MAPPING_QUERY_OUTPUT, *PHVMM_MAPPING_QUERY_OUTPUT;

typedef struct _HVMM_MAP_GPA_RANGE_INPUT {
	HANDLE PartitionHandle;
	UINT64 GpaStart;
	UINT64 Length;
} HVMM_MAP_GPA_RANGE_INPUT, *PHVMM_MAP_GPA_RANGE_INPUT;

typedef struct _HVMM_MAP_GPA_RANGE_OUTPUT {
	PVOID UserVa;
	UINT64 MappedBytes;
} HVMM_MAP_GPA_RANGE_OUTPUT, *PHVMM_MAP_GPA_RANGE_OUTPUT;

typedef struct _HVMM_UNMAP_GPA_RANGE_INPUT {
	PVOID UserVa;
} HVMM_UNMAP_GPA_RANGE_INPUT, *PHVMM_UNMAP_GPA_RANGE_INPUT;

//
// One guest physical run as reported by hvlib (InfoRun), in bytes.
//

typedef struct _HVMM_MAPPED_RUN {
	UINT64 GpaStart;
	UINT64 Length;
} HVMM_MAPPED_RUN, *PHVMM_MAPPED_RUN;

//
// One live mapping. Consecutive guest pages land at consecutive user VAs, so
// byte (GpaStart + n) is at UserVa[n].
//

typedef struct _HVMM_MAPPED_CHUNK {
	UINT64 GpaStart;
	UINT64 Length;
	PUCHAR UserVa;
} HVMM_MAPPED_CHUNK, *PHVMM_MAPPED_CHUNK;

typedef struct _HVMM_MAPPED_MEMORY {
	ULONG64 Partition;              // hvlib partition handle (opaque cookie)
	HANDLE VidPartitionHandle;      // from SdkGetData2(Partition, InfoPartitionHandle)
	HANDLE DeviceHandle;            // \\.\hvmm
	BOOLEAN OwnsDeviceHandle;       // TRUE if HvmmMappedOpen opened DeviceHandle
	READ_MEMORY_METHOD FallbackMethod;
	UINT64 MaxMapLength;            // largest single map request, from the handshake

	ULONG RunCount;
	PHVMM_MAPPED_RUN Runs;

	ULONG ChunkCount;               // sorted by GpaStart, non-overlapping
	PHVMM_MAPPED_CHUNK Chunks;

	UINT64 RunBytes;                // sum of run lengths
	UINT64 MappedBytes;             // sum of chunk lengths
	UINT64 MapCalls;                // IOCTLs issued while opening
	UINT64 FallbackReads;           // reads (or partial reads) served by hvlib
} HVMM_MAPPED_MEMORY, *PHVMM_MAPPED_MEMORY;

//
// Map every run of Partition. DeviceHandle may be an existing handle to \\.\hvmm
// (the LeechCore plugin already holds one); pass NULL to let the helper open its
// own. On failure nothing is left mapped and *Mapped is NULL; GetLastError() is
// ERROR_NOT_SUPPORTED when the loaded hvmm.sys did not answer the mapping query, in
// which case no map request was sent to it.
//

BOOLEAN HvmmMappedOpen(_In_ ULONG64 Partition, _In_opt_ HANDLE DeviceHandle, _Out_ PHVMM_MAPPED_MEMORY *Mapped);

//
// Copy Length bytes at guest physical address Gpa into Buffer. Bytes outside the
// mapping come from SdkReadPhysicalMemory with the fallback method. Returns FALSE
// only if a fallback read failed.
//

BOOLEAN HvmmMappedRead(_In_ PHVMM_MAPPED_MEMORY Mapped, _In_ UINT64 Gpa, _Out_writes_bytes_(Length) PVOID Buffer, _In_ UINT64 Length);

//
// Zero-copy access. Returns the user VA for Gpa and, in *AvailableBytes, how many
// contiguous bytes are readable from it. NULL if Gpa is not mapped.
//

PVOID HvmmMappedGetPointer(_In_ PHVMM_MAPPED_MEMORY Mapped, _In_ UINT64 Gpa, _Out_opt_ PUINT64 AvailableBytes);

//
// Probe the partition through the driver (map one page, unmap it). FALSE once
// the partition handle no longer resolves. It proves the partition object is
// still there, not that the guest still owns the mapped pages; pair it with
// SdkEnumPartitions when the VM's state matters.
//

BOOLEAN HvmmMappedIsAlive(_In_ PHVMM_MAPPED_MEMORY Mapped);

//
// Unmap everything, close the device handle if the helper opened it, free.
//

VOID HvmmMappedClose(_In_opt_ PHVMM_MAPPED_MEMORY Mapped);

//
// Drop-in replacement for SdkReadPhysicalMemory. Serves the read from a mapping
// opened with HvmmMappedOpen for that partition, or calls SdkReadPhysicalMemory
// with Method if none is open. Suitable for function tables that expect the
// SdkReadPhysicalMemory signature.
//

BOOLEAN HvmmMappedReadPhysicalMemory(_In_ ULONG64 PartitionHandle, _In_ UINT64 StartPosition, _In_ UINT64 ReadByteCount, _Inout_ PVOID ClientBuffer, _In_ READ_MEMORY_METHOD Method);

//
// Raw IOCTL wrappers for callers that manage their own ranges. Call
// HvmmQueryMappingSupport on the device handle first and send no map or unmap
// request unless it returned TRUE: another hvmm.sys build may give those IOCTL
// codes a different meaning. HvmmMappedOpen does this itself.
//

BOOLEAN HvmmQueryMappingSupport(_In_ HANDLE DeviceHandle, _Out_opt_ PUINT64 MaxMapLength);
BOOLEAN HvmmMapGpaRange(_In_ HANDLE DeviceHandle, _In_ HANDLE VidPartitionHandle, _In_ UINT64 GpaStart, _In_ UINT64 Length, _Out_ PVOID *UserVa, _Out_ PUINT64 MappedBytes);
BOOLEAN HvmmUnmapGpaRange(_In_ HANDLE DeviceHandle, _In_ PVOID UserVa);

#ifdef __cplusplus
};
#endif

#endif // __HVMM_MAPPED_MEMORY_H__
