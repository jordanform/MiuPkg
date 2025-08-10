#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
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
  Prints the header row for the ACPI table list.
*/
VOID
PrintAcpiTableHeader(VOID)
{

    // Clear the screen and set the header attributes
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));

    Print(L"  %-12s %-10s %-10s %-8s %-14s %-s\n", 
        L"ACPI Table", L"Address", L"Length", L"OEMID", L"OEM Table ID", L"Creator ID");

    // Restore default attribute
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
}

/**
  Prints the entries of a given root table (RSDT or XSDT).
  
  @param RootTable      A pointer to the root table (RSDT or XSDT).
  @param IsXsdt         TRUE if the RootTable is XSDT, FALSE if it's RSDT.
*/
VOID
PrintAcpiTableEntries(
  IN EFI_ACPI_DESCRIPTION_HEADER *RootTable,
  IN BOOLEAN IsXsdt
)
{
    UINTN EntryCount;
    
    if (IsXsdt) {
        UINT64 *EntryPtr = (UINT64*)((UINT8*)RootTable + sizeof(EFI_ACPI_DESCRIPTION_HEADER));
        EntryCount = (RootTable->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);
        for (UINTN i = 0; i < EntryCount; i++) {
            EFI_ACPI_DESCRIPTION_HEADER* TableHeader = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)EntryPtr[i];
            CHAR8 OemIdStr[7], OemTableIdStr[9], CreatorIdStr[5];
            
            CopyMem(OemIdStr, TableHeader->OemId, 6); OemIdStr[6] = '\0';
            CopyMem(OemTableIdStr, &TableHeader->OemTableId, 8); OemTableIdStr[8] = '\0';
            *(UINT32*)CreatorIdStr = TableHeader->CreatorId; CreatorIdStr[4] = '\0';
            
            Print(L"    %-10a   %-10X %-10X %-8a %-14a %-a\n", 
                (CHAR8*)&TableHeader->Signature, TableHeader, TableHeader->Length,
                OemIdStr, OemTableIdStr, CreatorIdStr
            );
        }
    } else { // This is an RSDT with 32-bit pointers
        UINT32 *EntryPtr = (UINT32*)((UINT8*)RootTable + sizeof(EFI_ACPI_DESCRIPTION_HEADER));
        EntryCount = (RootTable->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT32);
        for (UINTN i = 0; i < EntryCount; i++) {
            EFI_ACPI_DESCRIPTION_HEADER* TableHeader = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)EntryPtr[i];
            CHAR8 OemIdStr[7], OemTableIdStr[9], CreatorIdStr[5];
            
            CopyMem(OemIdStr, TableHeader->OemId, 6); OemIdStr[6] = '\0';
            CopyMem(OemTableIdStr, &TableHeader->OemTableId, 8); OemTableIdStr[8] = '\0';
            *(UINT32*)CreatorIdStr = TableHeader->CreatorId; CreatorIdStr[4] = '\0';
            
            Print(L"    %-10a   %-10X %-10X %-8a %-14a %-a\n", 
                (CHAR8*)&TableHeader->Signature, TableHeader, TableHeader->Length,
                OemIdStr, OemTableIdStr, CreatorIdStr
            );
        }
    }
}

/**
  Main entry point for the ACPI feature.
  Finds and lists all available ACPI tables from both RSDT and XSDT.
*/
VOID
ReadAcpiTables(VOID)
{
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;
    EFI_ACPI_DESCRIPTION_HEADER                  *Rsdt = NULL;
    EFI_ACPI_DESCRIPTION_HEADER                  *Xsdt = NULL;
    UINTN                                        SavedAttribute;

    // Save current text attribute
    SavedAttribute = gST->ConOut->Mode->Attribute;
    
    // FIX: Set colors to Light Gray on Blue background
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

    PrintAcpiTableHeader();
    
    if (EFI_ERROR(FindRsdp(&Rsdp))) {
        Print(L"Error: ACPI 2.0+ configuration table not found.\n");
        gST->ConOut->SetAttribute(gST->ConOut, SavedAttribute); // Restore attribute on exit
        return;
    }
    if (!IsValidChecksum(Rsdp, sizeof(EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER))) {
        Print(L"Error: ACPI RSDP checksum is invalid.\n");
        gST->ConOut->SetAttribute(gST->ConOut, SavedAttribute); // Restore attribute on exit
        return;
    }

    // --- Process RSDT ---
    Rsdt = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)Rsdp->RsdtAddress;
    if (Rsdt != NULL && IsValidChecksum(Rsdt, Rsdt->Length)) {
        // Set color to yellow for the "RSDT" title
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
        Print(L"RSDT\n");
        // Set color back for the header and entries
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        PrintAcpiTableEntries(Rsdt, FALSE);
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
        Print(L"\n");
    }

    // --- Process XSDT ---
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER*)Rsdp->XsdtAddress;
    if (Xsdt != NULL && IsValidChecksum(Xsdt, Xsdt->Length)) {
        // Set color to yellow for the "XSDT" title
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
        Print(L"XSDT\n");
        // Set color back for the header and entries
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        PrintAcpiTableEntries(Xsdt, TRUE);
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
    }

    // Restore original text attribute before returning
    gST->ConOut->SetAttribute(gST->ConOut, SavedAttribute);
}