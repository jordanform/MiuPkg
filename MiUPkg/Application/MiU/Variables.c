extern EFI_HANDLE gImageHandle;
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include "Variables.h"
#include "FileHelper.h"

#define MAX_VARIABLES     1024
#define MAX_NAME_CHARS    512
#define ITEMS_PER_PAGE    10

typedef struct {
  CHAR16    Name[MAX_NAME_CHARS];
  UINT32    Attributes;
  EFI_GUID  VendorGuid;
} VARIABLE_ENTRY;

/**
  Display the variable data in hex, allow cursor movement, and return on ESC.
*/
VOID
ShowVariableData(
  IN VARIABLE_ENTRY  *VarEntry
  )
{
  EFI_STATUS     Status;
  UINTN          DataSize = 0;
  UINT8         *DataBuf  = NULL;
  EFI_INPUT_KEY Key;
 
  UINTN          BytesPerLine = 16;
  UINTN          Lines;
  UINTN          Cursor      = 0;
  BOOLEAN        ExitView    = FALSE;

  // 1) Query for size first
  Status = gRT->GetVariable(
             VarEntry->Name,
             &VarEntry->VendorGuid,
             &VarEntry->Attributes,
             &DataSize,
             NULL
           );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"Unable to query size: %r\n", Status);
    return;
  }

  // 2) Allocate buffer and retrieve data
  DataBuf = AllocatePool(DataSize);
  if (DataBuf == NULL) {
    Print(L"Out of resources\n");
    return;
  }
  Status = gRT->GetVariable(
             VarEntry->Name,
             &VarEntry->VendorGuid,
             &VarEntry->Attributes,
             &DataSize,
             DataBuf
           );
  if (EFI_ERROR(Status)) {
    Print(L"Unable to read data: %r\n", Status);
    FreePool(DataBuf);
    return;
  }

  // 3) Enter navigation loop
  while (!ExitView) {
    // clear and redraw header
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    // Top line: Name on left, Size/Attr on right
    {
      CHAR16 AttrBuf[32] = L"";
      if (VarEntry->Attributes & EFI_VARIABLE_NON_VOLATILE)       StrCatS(AttrBuf, 32, L"NV ");
      if (VarEntry->Attributes & EFI_VARIABLE_BOOTSERVICE_ACCESS) StrCatS(AttrBuf, 32, L"BS ");
      if (VarEntry->Attributes & EFI_VARIABLE_RUNTIME_ACCESS)     StrCatS(AttrBuf, 32, L"RT");
      // print name + right-aligned info
      UINTN nameLen = StrLen(VarEntry->Name);
      gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
      Print(L"%s\n", VarEntry->Name);
      Print(L"%*sSize:0x%X Attr:%s\n",
            (gST->ConOut->Mode->CursorColumn > nameLen)
             ? (gST->ConOut->Mode->CursorColumn - nameLen - 1)
             : 0,
            L"",
            DataSize,
            AttrBuf);
    }

    //
    // --- Middle of screen: column header + hex dump with colored offsets ---
    //
    Lines = (DataSize + BytesPerLine - 1) / BytesPerLine;

    // 1) Column index header row print leading spaces for the row offset column
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
    Print(L"    "); // three chars: we'll align with "%02x: "
    for (UINTN col = 0; col < BytesPerLine; col++) {

        // highlight the selected column in white, others in red
        if (col == (Cursor % BytesPerLine)) {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        } else {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED,   EFI_BLUE));
        }

        Print(L"%02x ", col);
    }
    Print(L"\n");

    // 2) Each data row: row offset + bytes
    for (UINTN row = 0; row < Lines; row++) {
        UINTN base = row * BytesPerLine;

        // a) row offset in red, except the cursor's row in white
        if (row == (Cursor / BytesPerLine)) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED,   EFI_BLUE));
        }
        // print like "10: "
        Print(L"%02x: ", base);

        // b) the actual data bytes
        for (UINTN col = 0; col < BytesPerLine; col++) {
        UINTN idx = base + col;
        if (idx >= DataSize) {
            // past end, just blank
            Print(L"   ");
        } else {
            // selected byte: white on green, otherwise normal gray on blue
            if (idx == Cursor) {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_GREEN));
            } else {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
            }
            Print(L"%02x ", DataBuf[idx]);
        }
        }

        Print(L"\n");
    }

    // Bottom: prompt
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"\nUse Up/Down/Left/Right arrows to move, ESC to return\n");

    // wait for key
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    // Check if the key is ctrl+S (0x13)
    if (Key.UnicodeChar == 0x13) {
        // Save the variable to a file
        Status = SaveBytesToFile(
        gImageHandle,
        L"variable_dump.bin",
        DataBuf,
        DataSize
        );
        // Print the result
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        if (EFI_ERROR(Status)) {
            Print(L"\nSave failed: %r\n", Status);
        } else {
            Print(L"\nSaved to variable_dump.bin\n");
        }
        // Wait for a key before continuing
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
        continue;
    } 

    // navigation & exit
    if (Key.UnicodeChar == CHAR_NULL) {
      switch (Key.ScanCode) {
        case SCAN_LEFT:
          if (Cursor > 0) Cursor--;
          break;
        case SCAN_RIGHT:
          if (Cursor + 1 < DataSize) Cursor++;
          break;
        case SCAN_UP:
          if (Cursor >= BytesPerLine) Cursor -= BytesPerLine;
          break;
        case SCAN_DOWN:
          if (Cursor + BytesPerLine < DataSize) Cursor += BytesPerLine;
          break;
        case SCAN_ESC:
          ExitView = TRUE;
          break;
        default:
          break;
      }
    }
  }

  // restore default attributes & free
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
  FreePool(DataBuf);
}

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

    // Handle enter key or carriage return
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN || Key.UnicodeChar == CHAR_LINEFEED) {
        UINTN Index = CurrPage * ITEMS_PER_PAGE + CurrSel;
        ShowVariableData(&List[Index]);
        continue; // redraw the screen
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
  }

  // never reached
}