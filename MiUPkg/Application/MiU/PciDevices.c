#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PciIo.h>
#include "PciDevices.h"
#include <MiU.h>

PCI_ENTRY *mPciList   = NULL;
UINTN      mPciCount  = 0;
UINTN      mSelected  = 0;

CONST CHAR16*
GetPciDeviceName(
  IN UINT16 VendorId,
  IN UINT16 DeviceId
  )
{
  for (UINTN i = 0; mPciNameTable[i].Name != NULL; i++) {
    if (mPciNameTable[i].VendorId == VendorId && mPciNameTable[i].DeviceId == DeviceId) {
      return mPciNameTable[i].Name;
    }
  }
  return L"Unknown Device";
}

EFI_STATUS
EnumeratePciDevices (VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuf;
  UINTN       HandleCount;
  UINTN       Idx;

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiPciIoProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuf);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mPciList = AllocateZeroPool (HandleCount * sizeof (PCI_ENTRY));
  if (mPciList == NULL) {
    FreePool (HandleBuf);
    return EFI_OUT_OF_RESOURCES;
  }

  for (Idx = 0; Idx < HandleCount; ++Idx) {
    PCI_ENTRY *Entry = &mPciList[Idx];
    Entry->Handle   = HandleBuf[Idx];
    Status = gBS->HandleProtocol (HandleBuf[Idx],
                                  &gEfiPciIoProtocolGuid,
                                  (VOID **)&Entry->PciIo);
    if (EFI_ERROR(Status) || Entry->PciIo == NULL) {
      Print(L"[ERROR] HandleProtocol failed for handle %u: %r\n", Idx, Status);
      continue;
    }
    Status = Entry->PciIo->GetLocation (Entry->PciIo,
                                        (UINTN *)&Entry->Segment,
                                        (UINTN *)&Entry->Bus,
                                        (UINTN *)&Entry->Dev,
                                        (UINTN *)&Entry->Func);
    if (EFI_ERROR(Status)) {
      Print(L"[ERROR] GetLocation failed for handle %u: %r\n", Idx, Status);
      continue;
    }
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x00,
      1,
      &Entry->VendorId
    );
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x02,
      1,
      &Entry->DeviceId
    );    
  }
  mPciCount = HandleCount;
  FreePool (HandleBuf);
  return EFI_SUCCESS;
}
