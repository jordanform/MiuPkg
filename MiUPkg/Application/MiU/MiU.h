#ifndef _MIU_H_
#define _MIU_H_

#include <efi.h>

// Function Prototypes for your features
EFI_STATUS EnumeratePciDevices(VOID);
EFI_STATUS ReadSmbiosData(VOID);
EFI_STATUS ReadAcpiTables(VOID);
// Add more function prototypes here as you create new features

#endif // _MIU_H_