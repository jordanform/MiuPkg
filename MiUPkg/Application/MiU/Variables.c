//
// Variables.c
//
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include "Variables.h"

#define MAX_VARIABLES     1024
#define MAX_NAME_CHARS    512
#define ITEMS_PER_PAGE    10

typedef struct {
  CHAR16    Name[MAX_NAME_CHARS];
  UINT32    Attributes;
  EFI_GUID  VendorGuid;
} VARIABLE_ENTRY;


EFI_STATUS
ReadAllVariables(VOID)
{
  EFI_STATUS       Status;
  VARIABLE_ENTRY  *List;
  UINTN            VarCount = 0;

  //
  // 1) Gather all variable names + attributes
  //
  List = AllocateZeroPool(sizeof(*List) * MAX_VARIABLES);
  if (List == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Allocate a buffer for enumeration
  //
  UINTN   NameSize = MAX_NAME_CHARS * sizeof(CHAR16);
  CHAR16 *NameBuf  = AllocateZeroPool(NameSize);
  EFI_GUID Guid     = {0};

  //
  // First GetNextVariableName call to prime the pump
  //
  Status = gRT->GetNextVariableName(&NameSize, NameBuf, &Guid);
  while (!EFI_ERROR(Status) && VarCount < MAX_VARIABLES) {
    //
    // We now have NameBuf + Guid: pull its attributes
    //
    UINT32   Attr       = 0;
    UINTN    DataSize   = 0;
    //
    // DataSize = 0 + Data = NULL is enough to retrieve Attributes
    //
    gRT->GetVariable(NameBuf, &Guid, &Attr, &DataSize, NULL);

    //
    // Copy into our array
    //
    StrnCpyS(List[VarCount].Name, MAX_NAME_CHARS, NameBuf, MAX_NAME_CHARS-1);
    List[VarCount].Attributes = Attr;
    List[VarCount].VendorGuid = Guid;
    VarCount++;

    //
    // Prepare next call
    //
    NameSize = MAX_NAME_CHARS * sizeof(CHAR16);
    Status = gRT->GetNextVariableName(&NameSize, NameBuf, &Guid);
  }

  FreePool(NameBuf);

  if (VarCount == 0) {
    Print(L"\nNo UEFI variables found.\n");
    FreePool(List);
    return EFI_NOT_FOUND;
  }

  //
  // 2) Paging & selection loop
  //
  UINTN TotalPages = (VarCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  UINTN CurrPage   = 0;
  UINTN CurrSel    = 0;
  EFI_INPUT_KEY Key;

  for (;;) {

    UINTN Columns, Rows;
    gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);

    //
    // Clear screen
    //
    gST->ConOut->ClearScreen(gST->ConOut);

    //
    // Print header row: three colored blocks
    //
    gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_RED    | EFI_WHITE);
    Print(L"%-30s", L"UEFI Variable Name");
    gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_RED  | EFI_WHITE);
    Print(L" %-15s",  L"Attributes");
    gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_RED    | EFI_WHITE);

    // Pad the rest of the line and use one newline
    UINTN HeaderTextLength = 30 + 1 + 15 + 1 + StrLen(L"GUID");
    UINTN Padding = (Columns > HeaderTextLength) ? (Columns - HeaderTextLength) : 0;
    Print(L" %s%*s", L"GUID", Padding, L"");

    //
    // Print each line in this page
    //
    for (UINTN i = 0; i < ITEMS_PER_PAGE; i++) {
      UINTN Index = CurrPage * ITEMS_PER_PAGE + i;
      if (Index >= VarCount) {
        break;
      }

      // highlight selected line in green
      if (i == CurrSel) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_GREEN | EFI_BLUE);
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_BLUE  | EFI_WHITE);
      }

      //
      // build attribute string
      //
      CHAR16 AttrBuf[20] = L"";
      // Use StrCatS for CHAR16 strings
      if (List[Index].Attributes & EFI_VARIABLE_NON_VOLATILE)       StrCatS(AttrBuf, ARRAY_SIZE(AttrBuf), L"NV ");
      if (List[Index].Attributes & EFI_VARIABLE_BOOTSERVICE_ACCESS) StrCatS(AttrBuf, ARRAY_SIZE(AttrBuf), L"BS ");
      if (List[Index].Attributes & EFI_VARIABLE_RUNTIME_ACCESS)     StrCatS(AttrBuf, ARRAY_SIZE(AttrBuf), L"RT");

      //
      // Print name, attrs, GUID (%g prints a GUID)
      //
      Print(L"%-30s %-15s %g\n",
            List[Index].Name,
            AttrBuf,
            &List[Index].VendorGuid);
    }

    //
    // Footer: page indicator
    //
    gST->ConOut->SetAttribute(gST->ConOut, EFI_BACKGROUND_BLUE | EFI_WHITE);
    Print(L"\nPage %u/%u   (up/down select, PgUp/PgDn switch page, Esc to exit)\n",
          CurrPage + 1, TotalPages);

    //
    // Wait for and process key
    //
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) {
      // no key yet; wait for the next keystroke event
      UINTN Index;
      gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
      continue;
    }

    // Arrow keys come through ScanCode==0 for UnicodeChar, so:
    if (Key.UnicodeChar == CHAR_NULL) {
      switch (Key.ScanCode) {
        case SCAN_UP:
          if (CurrSel > 0) {
            CurrSel--;
          } else if (CurrPage > 0) {
            CurrPage--;
            CurrSel = ITEMS_PER_PAGE - 1;
          }
          break;

        case SCAN_DOWN:
          if ((CurrSel + 1 < ITEMS_PER_PAGE) &&
              ((CurrPage * ITEMS_PER_PAGE + CurrSel + 1) < VarCount))
          {
            CurrSel++;
          } else if (CurrPage + 1 < TotalPages) {
            CurrPage++;
            CurrSel = 0;
          }
          break;

        case SCAN_PAGE_UP:
          if (CurrPage > 0) {
            CurrPage--;
            CurrSel = 0;
          }
          break;

        case SCAN_PAGE_DOWN:
          if (CurrPage + 1 < TotalPages) {
            CurrPage++;
            CurrSel = 0;
          }
          break;

        case SCAN_ESC:
          FreePool(List);
          return EFI_SUCCESS;

        default:
          break;
      }
    } 
    // FIX: Removed redundant and incorrect check for SCAN_ESC as a UnicodeChar
  }

  // never reached
}