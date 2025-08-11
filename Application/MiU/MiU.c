#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
//#include <Library/ShellLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/SimpleTextInEx.h>
#include <MiU.h>
#include "Smbios.h"
#include "PciDevices.h"
#include "ACPI.h"
#include "Variables.h"
#include "IoSpace.h"
#include "ShowMemoryMap.h"

// Globals variable for input handling
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *mInputEx = NULL; 

// Forward declaration for our new function
STATIC VOID ShowHelpPopup(VOID);

/**
  Draw the device list on screen.
  The currently-selected row is rendered with a different background color.
*/
STATIC
VOID
DrawDeviceList (VOID)
{
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;  
  ConOut->ClearScreen(ConOut);

  // Draw header row with corrected spacing
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
  Print(
    L" %-48s %-15s %s\n",
    L"Name",
    L"Vendor:Device",
    L"Seg/Bus:Dev:Fun"
    );
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

  // Get each device
  for (UINTN Index = 0; Index < mPciCount; Index++) {
    PCI_ENTRY *E = &mPciList[Index];
    CONST CHAR16 *Name = GetPciDeviceName(E->VendorId, E->DeviceId);
    CHAR16 NameColumn[128];
    UnicodeSPrint(
      NameColumn,
      sizeof(NameColumn),
      L" %02X:%02X %s",
      E->Dev,
      E->Func,
      Name
    );

    // Set color for the current row based on whether it is selected
    if (Index == mSelected) {
      ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_GREEN));
    } else {
      ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    }

    Print(
      L"%-49s %04X:%04X       %02X/%02X:%02X:%02X\n",
      NameColumn,
      E->VendorId,
      E->DeviceId,
      0,
      E->Bus,
      E->Dev,
      E->Func
    );
  }

  // Restore default attribute
  ConOut->SetAttribute (ConOut, EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLUE));
}

/**
  Draws a popup window in the center of the screen with help text.
  It waits for a key press before returning.
*/
STATIC
VOID
ShowHelpPopup(VOID)
{
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;
    UINTN                           Columns, Rows;
    UINTN                           PopupTop, PopupLeft, PopupWidth, PopupHeight;
    EFI_INPUT_KEY                   Key;
    UINTN                           SavedCursorRow, SavedCursorColumn;
    UINTN                           SavedAttribute;

    // Save current cursor position and attribute
    SavedCursorColumn = ConOut->Mode->CursorColumn;
    SavedCursorRow = ConOut->Mode->CursorRow;
    SavedAttribute = ConOut->Mode->Attribute;

    // Get screen dimensions
    ConOut->QueryMode(ConOut, ConOut->Mode->Mode, &Columns, &Rows);

    // Define popup dimensions
    PopupWidth = 55;
    PopupHeight = 15;
    PopupLeft = (Columns - PopupWidth) / 2;
    PopupTop = (Rows - PopupHeight) / 2;

    // Set popup colors
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));

    // Clear the area for the popup
    for (UINTN i = 0; i < PopupHeight; i++) {
        ConOut->SetCursorPosition(ConOut, PopupLeft, PopupTop + i);
        Print(L"%*s", PopupWidth, L"");
    }

    // Draw the border using ASCII characters for compatibility
    ConOut->SetCursorPosition(ConOut, PopupLeft, PopupTop);
    Print(L"+-------------------------------------------------------+");
    for (UINTN i = 1; i < PopupHeight - 1; i++) {
        ConOut->SetCursorPosition(ConOut, PopupLeft, PopupTop + i);
        Print(L"|");
        ConOut->SetCursorPosition(ConOut, PopupLeft + PopupWidth - 1, PopupTop + i);
        Print(L"|");
    }
    ConOut->SetCursorPosition(ConOut, PopupLeft, PopupTop + PopupHeight - 1);
    Print(L"+-------------------------------------------------------+");

    // Print the help text
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLUE, EFI_LIGHTGRAY));
    ConOut->SetCursorPosition(ConOut, PopupLeft + 2, PopupTop + 1);
    Print(L"Help & Hotkeys");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
    
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 3);
    Print(L"F1 : Show PCI Device List (Current View)");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 4);
    Print(L"F2 : Read SMBIOS Data");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 5);
    Print(L"F3 : Read ACPI Tables");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 6);
    Print(L"F4 : Read All Variables");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 7);
    Print(L"F5 : Read I/O Space");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 8);
    Print(L"F6 : Show Memory Map");
    
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 10);
    Print(L"ENTER : View PCI Config Space");
    ConOut->SetCursorPosition(ConOut, PopupLeft + 4, PopupTop + 11);
    Print(L"ESC   : Quit");

    // Wait for a key press to close the popup
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    // Restore the original cursor position and attribute
    ConOut->SetAttribute(ConOut, SavedAttribute);
    ConOut->SetCursorPosition(ConOut, SavedCursorColumn, SavedCursorRow);
}

/**
  Main loop: use arrow keys to move the highlight, ENTER to view config space,
  ESC to quit the application.
*/
STATIC
VOID
MainLoop (VOID) {
  EFI_KEY_DATA  KeyData;
  BOOLEAN       ExitLoop = FALSE;

  // Perform the initial draw before starting the loop.
  DrawDeviceList();

  while (!ExitLoop) {
    BOOLEAN NeedRedraw = FALSE;
    EFI_STATUS Status;

    // Wait for a key to be pressed using the new protocol
    gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
    Status = mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
    if (EFI_ERROR(Status)) {
        continue; // Skip if there's an error reading the key
    }

    // check F1~F6 keys
    switch (KeyData.Key.ScanCode) {
      case SCAN_F1:
        // Redraw the device list
        NeedRedraw = TRUE;
        break;

      case SCAN_F2:
        ReadSmbiosData();
        Print(L"\nPress any key to return...");
        gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
        mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
        NeedRedraw = TRUE;
        break;

      case SCAN_F3:
        ReadAcpiTables();
        Print(L"\nPress any key to return...");
        gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
        mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
        NeedRedraw = TRUE;
        break;

      case SCAN_F4:
        ReadAllVariables();
        Print(L"\nPress any key to return...");
        gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
        mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
        NeedRedraw = TRUE;
        break;

      case SCAN_F5:
        #if defined(MDE_CPU_AARCH64)
          Print(L"Reading I/O space is not supported on ARM64 systems.\n");
        #else
          ReadIoSpace();
        #endif
        Print(L"\nPress any key to return...");
        gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
        mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
        NeedRedraw = TRUE;
        break;

      case SCAN_F6:
        ShowMemoryMap();
        Print(L"\nPress any key to return...");
        gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
        mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
        NeedRedraw = TRUE;
        break;

      // Handle arrow keys and ESC
      case SCAN_UP:
        if (mSelected > 0) { --mSelected; NeedRedraw = TRUE; }
        break;
      case SCAN_DOWN:
        if (mSelected + 1 < mPciCount) { ++mSelected; NeedRedraw = TRUE; }
        break;
      case SCAN_ESC:
        ExitLoop = TRUE;
        break;
      case SCAN_NULL:
        if (KeyData.Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          ShowPCIConfigSpace(&mPciList[mSelected]);
          NeedRedraw = TRUE;
        } else if (KeyData.Key.UnicodeChar == L'h' || KeyData.Key.UnicodeChar == L'H') {
          ShowHelpPopup();
          NeedRedraw = TRUE;
        }
        break;
      default:
        break;
    }

    // If the key was not handled, check for other keys
    if (KeyData.Key.ScanCode == SCAN_NULL) {
      switch (KeyData.Key.UnicodeChar) {
        case L'1': NeedRedraw = TRUE; break;
        case L'2':
          ReadSmbiosData();
          Print(L"\nPress any key to return...");
          gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
          mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
          NeedRedraw = TRUE;
          break;
        case L'3':
          ReadAcpiTables();
          Print(L"\nPress any key to return...");
          gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
          mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
          NeedRedraw = TRUE;
          break;
        case L'4':
          ReadAllVariables();
          Print(L"\nPress any key to return...");
          gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
          mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
          NeedRedraw = TRUE;
          break;
        case L'5':
        #if defined(MDE_CPU_AARCH64)
          Print(L"Reading I/O space is not supported on ARM64 systems.\n");
        #else
          ReadIoSpace();
        #endif
          Print(L"\nPress any key to return...");
          gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
          mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
          NeedRedraw = TRUE;
          break;
        case L'6':
          ShowMemoryMap();
          Print(L"\nPress any key to return...");
          gBS->WaitForEvent(1, &mInputEx->WaitForKeyEx, NULL);
          mInputEx->ReadKeyStrokeEx(mInputEx, &KeyData);
          NeedRedraw = TRUE;
          break;
        default:
          break;
      }
    }

    // If a key was pressed that changed the state, redraw the screen.
    if (NeedRedraw) {
      DrawDeviceList();
    }
  }
}

/**
  UEFI application entry point.
*/
EFI_STATUS
EFIAPI
UefiMain (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  // Get the main image handle
  gImageHandle = ImageHandle;

  // Initialize SimpleTextInputEx protocol for extended key input
  Status = gBS->LocateProtocol(&gEfiSimpleTextInputExProtocolGuid, NULL, (VOID **)&mInputEx);
  if (EFI_ERROR(Status) || mInputEx == NULL) {
    Print(L"SimpleTextInputEx protocol not found!\n");
    return EFI_UNSUPPORTED;
  }

  Status = EnumeratePciDevices();
  if (EFI_ERROR(Status)) {
    Print(L"PCI enumeration failed: %r\n", Status);
    return Status;
  }

  MainLoop();
  return EFI_SUCCESS;
}
