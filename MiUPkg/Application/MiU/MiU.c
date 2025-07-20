#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
//#include <Library/ShellLib.h>
#include <Protocol/PciIo.h>
#include <MiU.h>
#include "Smbios.h"

// Lookup human-readable name from Vendor and Device IDs
STATIC
CONST CHAR16*
GetPciDeviceName(
  IN UINT16 VendorId,
  IN UINT16 DeviceId
  )
{
  for (UINTN i = 0; mPciNameTable[i].Name != NULL; i++) {
    if (mPciNameTable[i].VendorId == VendorId && mPciNameTable[i].DeviceId == DeviceId) {
      return mPciNameTable[i].Name;
    }
  }
  return L"Unknown Device";
}

STATIC PCI_ENTRY *mPciList   = NULL;  // Pointer to dynamically-allocated PCI entry array
STATIC UINTN      mPciCount  = 0;     // Total number of devices found
STATIC UINTN      mSelected  = 0;     // Current index highlighted in the list

/**
  Enumerate all handles that support the EFI_PCI_IO_PROTOCOL and cache their
  location information (segment / bus / device / function).

  @retval EFI_SUCCESS           Enumeration succeeded.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure.
  @retval Others                Propagated status from LocateHandleBuffer().
*/
EFI_STATUS
EnumeratePciDevices (VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuf;
  UINTN       HandleCount;
  UINTN       Idx;

  // Locate all handles that support the EFI_PCI_IO_PROTOCOL
  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiPciIoProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuf);
  if (EFI_ERROR (Status)) {
    // Return error if no PCI devices found or other error
    return Status;
  }

  // Allocate memory for the PCI entry array
  mPciList = AllocateZeroPool (HandleCount * sizeof (PCI_ENTRY));
  if (mPciList == NULL) {
    // Free handle buffer and return if allocation fails
    FreePool (HandleBuf);
    return EFI_OUT_OF_RESOURCES;
  }

  // For each PCI device handle, get protocol and device info
  for (Idx = 0; Idx < HandleCount; ++Idx) {
    PCI_ENTRY *Entry = &mPciList[Idx];
    Entry->Handle   = HandleBuf[Idx];

    // Get the EFI_PCI_IO_PROTOCOL interface for this handle
    Status = gBS->HandleProtocol (HandleBuf[Idx],
                                  &gEfiPciIoProtocolGuid,
                                  (VOID **)&Entry->PciIo);
    if (EFI_ERROR(Status) || Entry->PciIo == NULL) {
      Print(L"[ERROR] HandleProtocol failed for handle %u: %r\n", Idx, Status);
      continue;
    }

    // Get the PCI device's segment, bus, device, and function numbers
    Status = Entry->PciIo->GetLocation (Entry->PciIo,
                                        (UINTN *)&Entry->Segment,
                                        (UINTN *)&Entry->Bus,
                                        (UINTN *)&Entry->Dev,
                                        (UINTN *)&Entry->Func);
    if (EFI_ERROR(Status)) {
      Print(L"[ERROR] GetLocation failed for handle %u: %r\n", Idx, Status);
      continue;
    }

    // Read Vendor ID from PCI config space offset 0x00
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x00,
      1,
      &Entry->VendorId
    );
    // Read Device ID from PCI config space offset 0x02
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x02,
      1,
      &Entry->DeviceId
    );    
  }

  // Store the number of PCI devices found
  mPciCount = HandleCount;
  // Free the handle buffer
  FreePool (HandleBuf);
  return EFI_SUCCESS;
}

/**
  Draw the command bar at the top of the screen with the given text.
  The command bar is typically used to display instructions or status.
  
  @param BarText  The text to display in the command bar.
*/
VOID
DrawCommandBar(CHAR16 *BarText)
{
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *ConOut = gST->ConOut;
    UINTN                             Columns, Rows;

    // pick your colours
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));

    // position at (0, 0)
    ConOut->SetCursorPosition(ConOut, 0, CMD_ROW);

    // make sure you clear the rest of the line
    ConOut->QueryMode(ConOut,
        ConOut->Mode->Mode, &Columns, &Rows);
    Print(L"%s%*s",
          BarText,
          (INT32)(Columns - StrLen(BarText)),
          L"");

    // restore your normal colours for content
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

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

  // draw the command bar again
  DrawCommandBar(L" File  Config  Edit  Go  Tools  System  Quit ");

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
  Read and display the 256-byte PCI configuration space of @p Entry as a
  16x16 hexadecimal dump.
*/
STATIC
VOID
ShowConfigSpace(PCI_ENTRY *Entry)
{
  UINT8         Data[256];
  EFI_INPUT_KEY Key;
  UINT8         RowSel = 0, ColSel = 0;
  BOOLEAN       ExitView = FALSE;
  BOOLEAN       ViewBits = FALSE;    

  // Read full 256-byte config space
  for (UINT32 Offset = 0; Offset < 256; Offset += 4) {
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint32,
      Offset,
      1,
      &Data[Offset]
    );
  }

  // Navigation loop
  while (!ExitView) {
    // Clear and reprint header
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(
      L"Device:%02x:%02x.%x   VID:DID = %04x:%04x\n\n",
      Entry->Bus,
      Entry->Dev,
      Entry->Func,
      Entry->VendorId,
      Entry->DeviceId
    );

    // Column labels
    // indent under the row-label column (4 chars: "00: ")
    Print(L"    ");
    for (UINT8 c = 0; c < 16; c++) {
      if (c == ColSel) {
        // selected column white on blue
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        // unselected red on blue
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
      }
      Print(L"%02x ", c);
    }
    Print(L"\n");

    // Draw the 16x16 table
    for (UINT8 Row = 0; Row < 16; Row++) {
      UINT8 Base = Row * 16;

      // Row label (offset)
      if (Row == RowSel) {
        // selected row white on blue
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        // other rows red on blue
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLUE));
      }
      Print(L"%02x: ", Base);

      // 16 bytes in this row
      for (UINT8 Col = 0; Col < 16; Col++) {
        if (Row == RowSel && Col == ColSel) {
          // selected cell white on green
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_GREEN));
        } else {
          // other cells lightgray on blue
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
        }
        Print(L"%02x ", Data[Base + Col]);
      }
      Print(L"\n");

      // inline bit-view under selected cell only
      if (ViewBits && Row == RowSel) {
        UINT8 Val = Data[RowSel * 16 + ColSel];
        // indent: 4 chars for "00: " + 3 chars per cell before ColSel
        UINTN indent = 4 + (ColSel * 3);
        for (UINTN i = 0; i < indent; i++) {
          Print(L" ");
        }
        // highlight the bit-string
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_GREEN));
        for (INTN b = 7; b >= 0; b--) {
          Print(L"%d", (Val >> b) & 1);
        }
        // restore normal text attr for next lines
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
        Print(L"\n");
      }
    }

    // Prompt
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"\nUse Up/Down/Left/Right arrows to move, ESC to return\n");

    // Wait and process a key
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    switch (Key.ScanCode) {
      case SCAN_UP:
        if (RowSel > 0) RowSel--;
        break;
      case SCAN_DOWN:
        if (RowSel < 15) RowSel++;
        break;
      case SCAN_LEFT:
        if (ColSel > 0) ColSel--;
        break;
      case SCAN_RIGHT:
        if (ColSel < 15) ColSel++;
        break;
      case SCAN_ESC:
        ExitView = TRUE;
        break;
      default:
      // detect Space by UnicodeChar, not ScanCode
        if (Key.UnicodeChar == L' ') {
          ViewBits = !ViewBits;
        }
        break;
    }
  }

  // Restore default before returning
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
}

/**
  Retrieves a string from an SMBIOS record.
  @param  SmbiosHeader  Pointer to the SMBIOS structure.
  @param  StringNumber  The 1-based index of the string to retrieve.
  @return               A pointer to the CHAR8 string, or a default string.
*/
STATIC
CONST CHAR8*
GetSmbiosString (
  IN SMBIOS_STRUCTURE         *SmbiosHeader,
  IN SMBIOS_TABLE_STRING      StringNumber
  )
{
  UINTN  i;
  CHAR8 *String;

  // Find the end of the formatted area
  String = (CHAR8 *)SmbiosHeader + SmbiosHeader->Length;

  // A string number of 0 means "no string"
  if (StringNumber == 0) {
    return "Not specified";
  }

  // Look for the referenced string
  for (i = 1; i < StringNumber; i++) {
    // Skip over the previous strings
    while (*String != 0) {
      String++;
    }
    String++; // Skip over the null terminator
  }

  // If we've gone past the end of the structure, return an error
  // (The SMBIOS structure is terminated by a double-null.)
  if (*String == 0) {
    return "Invalid String";
  }

  return String;
}

// Globals for the SMBIOS list
// ...existing code...

EFI_STATUS ReadAcpiTables(VOID) {
    Print(L"\n--- Reading ACPI Tables (Not yet implemented) ---\n");
    // Your ACPI reading logic will go here
    return EFI_UNSUPPORTED;
}

/**
  Main loop: use arrow keys to move the highlight, ENTER to view config space,
  ESC to quit the application.
*/
STATIC
VOID
MainLoop (VOID) {
  EFI_INPUT_KEY Key;
  BOOLEAN       ExitLoop = FALSE;

  // Perform the initial draw before starting the loop.
  DrawDeviceList();

  while (!ExitLoop) {
    BOOLEAN NeedRedraw = FALSE;

    // Wait for a key to be pressed.
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    // Process the key that was pressed.
    switch (Key.ScanCode) {
      case SCAN_UP:
        if (mSelected > 0) {
          --mSelected;
          NeedRedraw = TRUE; // Mark that a redraw is needed.
        }
        break;

      case SCAN_DOWN:
        if (mSelected + 1 < mPciCount) {
          ++mSelected;
          NeedRedraw = TRUE; // Mark that a redraw is needed.
        }
        break;

      case SCAN_ESC:
        ExitLoop = TRUE; // Signal to exit the main loop.
        break;

      case SCAN_NULL:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          ShowConfigSpace(&mPciList[mSelected]);
          NeedRedraw = TRUE; // Redraw the list when returning from the config space view.
        } else if (Key.UnicodeChar == L's' || Key.UnicodeChar == L'S') {
          ReadSmbiosData();
          NeedRedraw = TRUE;
        }
        break;
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
  EFI_STATUS Status = EnumeratePciDevices ();
  if (EFI_ERROR (Status)) {
    Print (L"PCI enumeration failed: %r\n", Status);
    return Status;
  }

  MainLoop ();
  return EFI_SUCCESS;
}
