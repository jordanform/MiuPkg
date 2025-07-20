#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>

// PCI device entry structure
typedef struct {
  EFI_HANDLE            Handle;
  EFI_PCI_IO_PROTOCOL *PciIo;
  UINT16               Segment;
  UINT16               Bus;
  UINT16               Dev;
  UINT16               Func;
  UINT16               VendorId;
  UINT16               DeviceId;
} PCI_ENTRY;

// PCI device enumeration
EFI_STATUS EnumeratePciDevices(VOID);

// PCI device list globals
extern PCI_ENTRY *mPciList;
extern UINTN      mPciCount;
extern UINTN      mSelected;

// Lookup human-readable name from Vendor and Device IDs
CONST CHAR16* GetPciDeviceName(IN UINT16 VendorId, IN UINT16 DeviceId);
