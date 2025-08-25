#include "ShowBootOption.h"
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include "FileHelper.h"

/**
  Display all Boot#### variables in the system.

  @retval EFI_SUCCESS     Successfully displayed all boot options.
  @retval Others          Failed to get or display boot options.
**/
typedef struct {
  CHAR16    Name[16];       // Boot#### format
  CHAR16    Description[256];
  UINT32    Attributes;
  UINTN     DataSize;
  UINT8     *Data;
} BOOT_OPTION_ENTRY;

STATIC
VOID
ShowBootOptionData(
  IN BOOT_OPTION_ENTRY  *BootEntry
  )
{
  EFI_INPUT_KEY Key;
  UINTN         BytesPerLine = 16;
  UINTN         Lines;
  UINTN         Cursor = 0;
  BOOLEAN       ExitView = FALSE;

  while (!ExitView) {
    // Clear and redraw header
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));

    // Top lines: Name and Description
    Print(L"%s\n", BootEntry->Name);
    Print(L"%s\n", BootEntry->Description);
    Print(L"Size: 0x%X\n", BootEntry->DataSize);

    // Print hex dump with headers
    Lines = (BootEntry->DataSize + BytesPerLine - 1) / BytesPerLine;

    // Column headers
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
    Print(L"    ");
    for (UINTN col = 0; col < BytesPerLine; col++) {
      if (col == (Cursor % BytesPerLine)) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
      }
      Print(L"%02x ", col);
    }
    Print(L"\n");

    // Data rows
    for (UINTN row = 0; row < Lines; row++) {
      UINTN base = row * BytesPerLine;

      if (row == (Cursor / BytesPerLine)) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
      }
      Print(L"%02x: ", base);

      for (UINTN col = 0; col < BytesPerLine; col++) {
        UINTN idx = base + col;
        if (idx >= BootEntry->DataSize) {
          Print(L"   ");
        } else {
          if (idx == Cursor) {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_GREEN));
          } else {
            gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
          }
          Print(L"%02x ", BootEntry->Data[idx]);
        }
      }
      Print(L"\n");
    }

    // Bottom: prompt
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"\nUse Up/Down/Left/Right arrows to move, ESC to return\n");

    // Wait for key
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    // Navigation
    if (Key.UnicodeChar == CHAR_NULL) {
      switch (Key.ScanCode) {
        case SCAN_LEFT:
          if (Cursor > 0) Cursor--;
          break;
        case SCAN_RIGHT:
          if (Cursor + 1 < BootEntry->DataSize) Cursor++;
          break;
        case SCAN_UP:
          if (Cursor >= BytesPerLine) Cursor -= BytesPerLine;
          break;
        case SCAN_DOWN:
          if (Cursor + BytesPerLine < BootEntry->DataSize) Cursor += BytesPerLine;
          break;
        case SCAN_ESC:
          ExitView = TRUE;
          break;
        default:
          break;
      }
    }
  }

  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
}

EFI_STATUS
EFIAPI
ShowBootOptions (
  VOID
  )
{
  EFI_STATUS         Status;
  UINT16             BootOrder[100];
  UINTN             BootOrderSize;
  BOOT_OPTION_ENTRY *BootList;
  UINTN             BootCount = 0;
  EFI_INPUT_KEY     Key;
  UINTN             CurrentSelection = 0;
  BOOLEAN           ExitMenu = FALSE;

  // Get the BootOrder variable
  BootOrderSize = sizeof(BootOrder);
  Status = gRT->GetVariable(
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &BootOrderSize,
                  BootOrder
                  );

  if (EFI_ERROR(Status)) {
    Print(L"Failed to get BootOrder variable: %r\n", Status);
    return Status;
  }

  // Allocate array for boot options
  UINTN MaxBootOptions = BootOrderSize / sizeof(UINT16);
  BootList = AllocateZeroPool(sizeof(BOOT_OPTION_ENTRY) * MaxBootOptions);
  if (BootList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Read all boot options
  for (UINTN i = 0; i < MaxBootOptions; i++) {
    CHAR16 BootVarName[16];
    UnicodeSPrint(BootVarName, sizeof(BootVarName), L"Boot%04x", BootOrder[i]);
    
    // Copy the name
    StrCpyS(BootList[BootCount].Name, 16, BootVarName);
    
    // Get the boot option data
    UINTN VarSize = 0;
    Status = gRT->GetVariable(
                    BootVarName,
                    &gEfiGlobalVariableGuid,
                    &BootList[BootCount].Attributes,
                    &VarSize,
                    NULL
                    );

    if (Status == EFI_BUFFER_TOO_SMALL && VarSize > 0) {
      BootList[BootCount].Data = AllocatePool(VarSize);
      BootList[BootCount].DataSize = VarSize;
      
      if (BootList[BootCount].Data != NULL) {
        Status = gRT->GetVariable(
                      BootVarName,
                      &gEfiGlobalVariableGuid,
                      &BootList[BootCount].Attributes,
                      &VarSize,
                      BootList[BootCount].Data
                      );

        if (!EFI_ERROR(Status)) {
          // Copy description (starts after Attributes(4) + FilePathListLength(2))
          CHAR16 *Desc = (CHAR16 *)(BootList[BootCount].Data + 6);
          StrCpyS(BootList[BootCount].Description, 256, Desc);
          BootCount++;
        } else {
          FreePool(BootList[BootCount].Data);
        }
      }
    }
  }

  // Main menu loop
  while (!ExitMenu) {
    // Clear screen and set colors
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
    Print(L"=== Boot Options ===                \n\n");

    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));

    // Display all boot options
    for (UINTN i = 0; i < BootCount; i++) {
      if (i == CurrentSelection) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_GREEN));
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      }
      Print(L"%s: %s\n", BootList[i].Name, BootList[i].Description);
    }

    // Reset color to blue background for the instruction text
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"\nUse Up/Down to select, Enter to view details, ESC to exit\n");

    // Wait for key
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      // Show details of selected boot option
      ShowBootOptionData(&BootList[CurrentSelection]);
    } else if (Key.UnicodeChar == CHAR_NULL) {
      switch (Key.ScanCode) {
        case SCAN_UP:
          if (CurrentSelection > 0) {
            CurrentSelection--;
          } else {
            // Wrap to bottom
            CurrentSelection = BootCount - 1;
          }
          break;
        case SCAN_DOWN:
          if (CurrentSelection + 1 < BootCount) {
            CurrentSelection++;
          } else {
            // Wrap to top
            CurrentSelection = 0;
          }
          break;
        case SCAN_ESC:
          ExitMenu = TRUE;
          break;
        default:
          break;
      }
    }
  }

  // Cleanup
  for (UINTN i = 0; i < BootCount; i++) {
    if (BootList[i].Data != NULL) {
      FreePool(BootList[i].Data);
    }
  }
  FreePool(BootList);

  return EFI_SUCCESS;
}
