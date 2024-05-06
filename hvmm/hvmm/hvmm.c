#include "hvmm.h"

extern PVOID pPsGetCurrentProcessOrig;
extern PVOID pSavedPsGetCurrentProcessFunction;
extern BOOLEAN bIsPsGetCurrentPsPatched;

NTSTATUS DeviceControlRoutine( IN PDEVICE_OBJECT fdo, IN PIRP Irp );
VOID     UnloadRoutine(IN PDRIVER_OBJECT DriverObject);
NTSTATUS Create_File_IRPprocessing(IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS Close_HandleIRPprocessing(IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS ReadWrite_IRPhandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp);

KSPIN_LOCK MySpinLock;

#define HVMM_MEMORY_TAG 'mmvH'

PVOID HvmmPoolAlloc(SIZE_T size)
{
	PVOID pPool = ExAllocatePoolZero(NonPagedPool, size, HVMM_MEMORY_TAG);
	if (!pPool) {
		KDbgPrintString("ExAllocatePoolZero failed");
		return NULL;
	}
	RtlZeroMemory(pPool, size);
	return pPool;
}

PVOID HvmmPoolFree(PVOID p)
{
	if (p)
		ExFreePoolWithTag(p, HVMM_MEMORY_TAG);

	return NULL;
}

NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath  )
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT  fdo;
	UNICODE_STRING  devName;
	PHVMM_DEVICE_EXTENSION dx;
	UNICODE_STRING symLinkName;
	UNICODE_STRING DeviceSDDLString;
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = UnloadRoutine;
	DriverObject->MajorFunction[IRP_MJ_CREATE]= Create_File_IRPprocessing;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = Close_HandleIRPprocessing;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]= DeviceControlRoutine;
    DriverObject->MajorFunction[IRP_MJ_READ] = ReadWrite_IRPhandler;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = ReadWrite_IRPhandler;

	RtlInitUnicodeString( &devName, L"\\Device\\hvmm" );
	RtlInitUnicodeString(&DeviceSDDLString, DEVICE_SDDL);

	status = IoCreateDeviceSecure(DriverObject,
                            sizeof(HVMM_DEVICE_EXTENSION),
                            &devName, 
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE, 
							&DeviceSDDLString,
							NULL,
                            &fdo);

	if(!NT_SUCCESS(status)) return status;

	dx = (PHVMM_DEVICE_EXTENSION)fdo->DeviceExtension;
	dx->fdo = fdo;  

	#define   SYM_LINK_NAME   L"\\DosDevices\\hvmm"

	RtlInitUnicodeString( &symLinkName, SYM_LINK_NAME );
	dx->ustrSymLinkName = symLinkName;
	
	status = IoCreateSymbolicLink( &symLinkName, &devName );
	if (!NT_SUCCESS(status))
	{ 
		KDbgLog("Error IoCreateSymbolicLink", status);
        IoDeleteDevice( fdo );
		return status;
    } 
	KDbgPrintString("hvmm was loaded succesfully");
    return status;
}

NTSTATUS CompleteIrp( PIRP Irp, NTSTATUS status, ULONG info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return status;
}

NTSTATUS ReadWrite_IRPhandler( IN PDEVICE_OBJECT fdo, IN PIRP Irp )
{
	ULONG BytesTxd = 0;
	NTSTATUS status = STATUS_SUCCESS; 
	UNREFERENCED_PARAMETER(fdo);
	return CompleteIrp(Irp,status,BytesTxd);
}

NTSTATUS Create_File_IRPprocessing(IN PDEVICE_OBJECT fdo,IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(fdo);
	return CompleteIrp(Irp,STATUS_SUCCESS,0); 
}

NTSTATUS Close_HandleIRPprocessing(IN PDEVICE_OBJECT fdo,IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(fdo);
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}

NTSTATUS DeviceControlRoutine( IN PDEVICE_OBJECT fdo, IN PIRP Irp )
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG BytesTxd =0; 
	UNREFERENCED_PARAMETER(fdo);
	PCHAR pInputBuffer, pOutputBuffer;
	PIO_STACK_LOCATION IrpStack=IoGetCurrentIrpStackLocation(Irp);
	ULONG uOutBufLen, uInputBufLen;
	ULONG ControlCode =	IrpStack->Parameters.DeviceIoControl.IoControlCode;

	uOutBufLen = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
    uInputBufLen = IrpStack->Parameters.DeviceIoControl.InputBufferLength;

    if (!uOutBufLen || !uInputBufLen)
    {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }

    pInputBuffer = Irp->AssociatedIrp.SystemBuffer;
    pOutputBuffer = Irp->AssociatedIrp.SystemBuffer;

	if (!pInputBuffer) {
		KDbgPrintString("uOutBufLen is too big");
	}

	if (uOutBufLen > USER_BUFFER_LIMIT)
	{
		KDbgPrintString("uOutBufLen is too big");
		uOutBufLen = USER_BUFFER_LIMIT;
	}
		
    if (uInputBufLen > USER_BUFFER_LIMIT)
	{ 
		KDbgPrintString("uInputBufLen is too big");
		uInputBufLen = USER_BUFFER_LIMIT;
	}

	//DbgBreakPoint();
	//KDbgLog("IOCTL", ControlCode);
	switch(ControlCode) {
    case IOCTL_HV_READ_GPA:
    {
        if (VidHvReadGpa(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
	case IOCTL_VID_INTERNAL_READ_MEMORY:
	{
		if (VidInternalReadMemory(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}
    case IOCTL_VID_QUERY_INFO:
    {
        if (VidQueryInformation(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
    case IOCTL_GET_MBLOCK_FROM_GPA:
    {
        if (VidGetGparBlockInfoFromGPA(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
	case IOCTL_GET_FRIENDLY_PARTIION_NAME:
	{
        if (VidGetFriendlyPartitionName(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	} 
	case IOCTL_GET_ACTIVE_PARTITIONS:
	{
		EnumActivePartitionID();
		BytesTxd = uOutBufLen;
		break;
	}
    case IOCTL_HV_WRITE_GPA:
    {
        if (VidHvWriteGpa(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
    case IOCTL_HV_TRANSLATE_VA:
    {
        if (VidTranslateGvatoGpa(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
    case IOCTL_HV_READ_REG:
    {
        if (VidReadVpRegisters(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
	case IOCTL_HV_WRITE_REG:
	{
		if (VidWriteVpRegisters(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}
    case IOCTL_HV_PATCH_GETPROCESS:
    {
		if (VidPatchPsGetCurrentProcess(pInputBuffer, uOutBufLen) == TRUE) {
            BytesTxd = uOutBufLen;
        }
        else {
            BytesTxd = 0;
        }
        break;
    }
	case IOCTL_HV_RESTORE_GETPROCESS:
	{
		if (VidRestorePsGetCurrentProcess() == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}
	case IOCTL_INJECT_VIDAUX_DLL:
	{
		if (VidInjectDllToVmwp(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}
	case IOCTL_DISABLE_VMWP_MITIGATIONS:
	{
		if (VidIOCTLDisableProcessProtection(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}
	case IOCTL_ENABLE_VMWP_MITIGATIONS:
	{
		if (VidIOCTLEnableProcessProtection(pInputBuffer, uOutBufLen) == TRUE) {
			BytesTxd = uOutBufLen;
		}
		else {
			BytesTxd = 0;
		}
		break;
	}

	default: status = STATUS_INVALID_DEVICE_REQUEST;
	}
End:
return CompleteIrp(Irp,status,BytesTxd); 
}


VOID UnloadRoutine(IN PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_OBJECT	pNextDevObj;
	int i;

	VidRestorePsGetCurrentProcess();
        
	pNextDevObj = pDriverObject->DeviceObject;

	for(i=0; pNextDevObj!=NULL; i++)
	{
		PHVMM_DEVICE_EXTENSION dx =
				(PHVMM_DEVICE_EXTENSION)pNextDevObj->DeviceExtension;
		UNICODE_STRING *pLinkName = & (dx->ustrSymLinkName);
		pNextDevObj = pNextDevObj->NextDevice;
		IoDeleteSymbolicLink(pLinkName);
		IoDeleteDevice(dx->fdo);
	}

	KDbgPrintString("hvmm was unloaded succesfully");
}