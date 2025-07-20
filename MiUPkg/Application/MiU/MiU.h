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

STATIC PCI_NAME_ENTRY mPciNameTable[] = {
  { 0x8086, 0x1237, L"Intel 82441FX MARS Pentium Pro to PCI" },
  { 0x8086, 0x7000, L"Intel 82371SB ISA bridge" },
  { 0x8086, 0x7010, L"Intel Triton PIIX3 IDE controller" },
  { 0x8086, 0x7113, L"Intel 82371AB Power Management Bridge" },
  { 0x8086, 0x100E, L"Intel Ethernet controller" },
  { 0x1234, 0x1111, L"VGA controller" },
  { 0,      0,      NULL }
};

// PCI device entry
typedef struct {
  EFI_HANDLE             Handle;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  UINT8                  Segment;
  UINT8                  Bus;
  UINT8                  Dev;
  UINT8                  Func;
  UINT16                 VendorId;
  UINT16                 DeviceId;
} PCI_ENTRY;

// Function that enumerates PCI devices
EFI_STATUS EnumeratePciDevices(VOID);
// Function that read smbios data
VOID ReadSmbiosData(VOID);
EFI_STATUS ReadAcpiTables(VOID);
// Add more function prototypes here as you create new features

#endif // _MIU_H_