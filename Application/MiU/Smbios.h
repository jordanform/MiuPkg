#pragma once
#include <Uefi.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>

// Forward declaration for SMBIOS_ENTRY struct
typedef struct {
  EFI_SMBIOS_HANDLE      Handle;
  EFI_SMBIOS_TABLE_HEADER *Header;
} SMBIOS_ENTRY;

// Main entry point for SMBIOS feature
VOID ReadSmbiosData(VOID);
