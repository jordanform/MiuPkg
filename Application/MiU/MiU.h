#ifndef _MIU_H_
#define _MIU_H_

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/PciIo.h>

#define MAX_DEVICES  256
#define CMD_ROW      0
#define CONTENT_ROW  1

// PCI device name mapping
typedef struct {
  UINT16     VendorId;
  UINT16     DeviceId;
  CHAR16    *Name;
} PCI_NAME_ENTRY;

/**
  Returns the name of a PCI device based on its VendorId and DeviceId.
  
  @param  VendorId   The Vendor ID of the PCI device.
  @param  DeviceId   The Device ID of the PCI device.
  
  @return A pointer to the device name string, or "Unknown Device" if not found.
*/
CONST CHAR16 *
GetPciDeviceName (
  IN UINT16 VendorId,
  IN UINT16 DeviceId
  );

#endif // _MIU_H_