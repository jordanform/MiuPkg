#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/Acpi.h>
#include "ACPI.h"

//
// GUID for the ACPI 2.0 or later table, used to find the RSDP
//
#define ACPI_20_TABLE_GUID \
  {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81}}

/**
  Calculates and verifies the checksum of an ACPI table.
  
  @param  Table   A pointer to the table to verify.
  @param  Length  The length of the table in bytes.

  @retval TRUE    The checksum is valid.
  @retval FALSE   The checksum is not valid.
*/
BOOLEAN
IsValidChecksum(
  IN VOID    *Table,
  IN UINTN   Length
  )
{
  UINT8 *Bytes = (UINT8*)Table;
  UINT8 Sum = 0;
  for (UINTN i = 0; i < Length; i++) {
    Sum += Bytes[i];
  }
  return (Sum == 0);
}

/**
  Finds the ACPI Root System Description Pointer (RSDP) from the UEFI System Table.
  
  @param  RsdpPtr   A pointer to a pointer that will be updated with the RSDP address.

  @retval EFI_SUCCESS   The RSDP was found and the RsdpPtr was updated.
  @retval EFI_NOT_FOUND If the ACPI table GUID was not found.
*/
EFI_STATUS
FindRsdp(
  OUT EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER **RsdpPtr
  )
{
  EFI_GUID Acpi20Guid = ACPI_20_TABLE_GUID;
  
  // Iterate through the UEFI Configuration Table to find the ACPI 2.0+ table
  for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
    if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &Acpi20Guid)) {
      *RsdpPtr = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER*)gST->ConfigurationTable[i].VendorTable;
      return EFI_SUCCESS;
    }
  }
  
  return EFI_NOT_FOUND;
}


/**
  Main entry point for the ACPI feature.
  This function finds and lists all available ACPI tables, handling errors internally.
*/
VOID
ReadAcpiTables(VOID)
{
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;
    EFI_ACPI_DESCRIPTION_HEADER                  *Xsdt = NULL;
    UINT64                                       *EntryPtr;
    UINTN                                        EntryCount;

    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"--- Reading ACPI Tables ---\n\n");

    // 1. Find the Root System Description Pointer (RSDP)
    if (EFI_ERROR(FindRsdp(&Rsdp))) {
        Print(L"Error: ACPI 2.0+ configuration table not found.\n");
        return;
    }

    // 2. Validate the RSDP checksum
    if (!IsValidChecksum(Rsdp, sizeof(EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER))) {
        Print(L"Error: ACPI RSDP checksum is invalid.\n");
        return;
    }

    Print(L"RSDP found at 0x%p. Revision: %d\n", Rsdp, Rsdp->Revision);
    
    // 3. Get the address of the XSDT (Extended System Description Table)
    //    The XSDT is the 64-bit version of the table list.
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER*)Rsdp->XsdtAddress;
    if (Xsdt == NULL) {
        Print(L"Error: XSDT address is NULL.\n");
        return;
    }

    // 4. Validate the XSDT checksum
    if (!IsValidChecksum(Xsdt, Xsdt->Length)) {
        Print(L"Error: ACPI XSDT checksum is invalid.\n");
        return;
    }

    Print(L"XSDT found at 0x%p. Length: %d bytes.\n\n", Xsdt, Xsdt->Length);
    Print(L"Listing all ACPI tables found in XSDT:\n");
    Print(L"------------------------------------------\n");
    Print(L"  Index | Address            | Signature | Length\n");
    Print(L"------------------------------------------\n");

    // 5. Parse the XSDT to find all other tables
    EntryPtr = (UINT64*)((UINT8*)Xsdt + sizeof(EFI_ACPI_DESCRIPTION_HEADER));
    EntryCount = (Xsdt->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);

    for (UINTN i = 0; i < EntryCount; i++) {
        EFI_ACPI_DESCRIPTION_HEADER* TableHeader = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)EntryPtr[i];
        
        // Convert the 32-bit signature to a printable string
        CHAR8 SignatureStr[5];
        *(UINT32*)SignatureStr = TableHeader->Signature;
        SignatureStr[4] = '\0'; // Null-terminate the string

        // Validate the checksum of each table before printing
        BOOLEAN IsValid = IsValidChecksum(TableHeader, TableHeader->Length);

        Print(L"   %2d   | 0x%016p | %a        | %d bytes %s\n",
            i,
            TableHeader,
            SignatureStr,
            TableHeader->Length,
            IsValid ? L"" : L"(Invalid Checksum!)"
        );
    }
    Print(L"------------------------------------------\n");
}