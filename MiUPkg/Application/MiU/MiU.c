#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
//#include <Library/ShellLib.h>
#include <Protocol/PciIo.h>
#include <MiU.h>

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
STATIC SMBIOS_ENTRY *mSmbiosList = NULL;
STATIC UINTN         mSmbiosCount = 0;
STATIC UINTN         mSmbiosSelected = 0;

/**
  Returns a human-readable name for a given SMBIOS type ID.
*/
STATIC
CONST CHAR16*
GetSmbiosTypeName(
  IN UINT8 Type
  )
{
  switch(Type) {
    case SMBIOS_TYPE_BIOS_INFORMATION:                return L"BIOS Information";
    case SMBIOS_TYPE_SYSTEM_INFORMATION:              return L"System Information";
    case SMBIOS_TYPE_BASEBOARD_INFORMATION:           return L"Baseboard Information";
    case SMBIOS_TYPE_SYSTEM_ENCLOSURE:                return L"System Enclosure or Chassis";
    case SMBIOS_TYPE_PROCESSOR_INFORMATION:           return L"Processor Information";
    case SMBIOS_TYPE_MEMORY_CONTROLLER_INFORMATION:   return L"Memory Controller Information";
    case SMBIOS_TYPE_MEMORY_MODULE_INFORMATON:        return L"Memory Module Information";
    case SMBIOS_TYPE_CACHE_INFORMATION:               return L"Cache Information";
    case SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION:      return L"Port Connector Information";
    case SMBIOS_TYPE_SYSTEM_SLOTS:                    return L"System Slots";
    case SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION:    return L"On Board Devices Information";
    case SMBIOS_TYPE_OEM_STRINGS:                     return L"OEM Strings";
    case SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS:    return L"System Configuration Options";
    case SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION:       return L"BIOS Language Information";
    case SMBIOS_TYPE_GROUP_ASSOCIATIONS:              return L"Group Associations";
    case SMBIOS_TYPE_SYSTEM_EVENT_LOG:                return L"System Event Log";
    case SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY:           return L"Physical Memory Array";
    case SMBIOS_TYPE_MEMORY_DEVICE:                   return L"Memory Device";
    case SMBIOS_TYPE_32BIT_MEMORY_ERROR_INFORMATION:  return L"32-bit Memory Error Information";
    case SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS:     return L"Memory Array Mapped Address";
    case SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS:    return L"Memory Device Mapped Address";
    case SMBIOS_TYPE_BUILT_IN_POINTING_DEVICE:        return L"Built-in Pointing Device";
    case SMBIOS_TYPE_PORTABLE_BATTERY:                return L"Portable Battery";
    case SMBIOS_TYPE_SYSTEM_RESET:                    return L"System Reset";
    case SMBIOS_TYPE_HARDWARE_SECURITY:               return L"Hardware Security";
    case SMBIOS_TYPE_SYSTEM_POWER_CONTROLS:           return L"System Power Controls";
    case SMBIOS_TYPE_VOLTAGE_PROBE:                   return L"Voltage Probe";
    case SMBIOS_TYPE_COOLING_DEVICE:                  return L"Cooling Device";
    case SMBIOS_TYPE_TEMPERATURE_PROBE:               return L"Temperature Probe";
    case SMBIOS_TYPE_ELECTRICAL_CURRENT_PROBE:        return L"Electrical Current Probe";
    case SMBIOS_TYPE_OUT_OF_BAND_REMOTE_ACCESS:       return L"Out-of-Band Remote Access";
    case SMBIOS_TYPE_BOOT_INTEGRITY_SERVICE:          return L"Boot Integrity Services (BIS)";
    case SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION:         return L"System Boot Information";
    case SMBIOS_TYPE_64BIT_MEMORY_ERROR_INFORMATION:  return L"64-bit Memory Error Information";
    case SMBIOS_TYPE_MANAGEMENT_DEVICE:               return L"Management Device";
    case SMBIOS_TYPE_MANAGEMENT_DEVICE_COMPONENT:     return L"Management Device Component";
    case SMBIOS_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA:return L"Management Device Threshold Data";
    case SMBIOS_TYPE_MEMORY_CHANNEL:                  return L"Memory Channel";
    case SMBIOS_TYPE_IPMI_DEVICE_INFORMATION:         return L"IPMI Device Information";
    case SMBIOS_TYPE_SYSTEM_POWER_SUPPLY:             return L"System Power Supply";
    case SMBIOS_TYPE_ADDITIONAL_INFORMATION:          return L"Additional Information";
    case SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION: return L"Onboard Devices Extended Info";
    case SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE: return L"Management Controller Host I/F";
    case SMBIOS_TYPE_INACTIVE:                        return L"Inactive";
    case SMBIOS_TYPE_END_OF_TABLE:                    return L"End-of-Table";
    default:                                          return L"Unknown or OEM-specific Type";
  }
}

/**
  Displays the detailed information for a single SMBIOS record.
*/
STATIC
VOID
ShowSmbiosRecordDetail(
  IN SMBIOS_ENTRY *Entry
  )
{
  EFI_INPUT_KEY Key;

  // Set the desired attribute and clear the entire screen with it.
  // This fills the whole screen with a blue background.
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  gST->ConOut->ClearScreen(gST->ConOut);
  
  Print(L"--- SMBIOS Record Detail ---\n");
  Print(L"Type: %d (%s)\n", Entry->Header->Type, GetSmbiosTypeName(Entry->Header->Type));
  Print(L"Handle: 0x%04X\n", Entry->Handle);
  Print(L"Length: 0x%02X\n\n", Entry->Header->Length);

  // Specific decoders can be added here. For now, we show a hex dump for all types.
  // This gives you a foundation to build on.
  switch(Entry->Header->Type) {
    case SMBIOS_TYPE_SYSTEM_INFORMATION: {
      SMBIOS_TABLE_TYPE1 *Rec = (SMBIOS_TABLE_TYPE1 *)Entry->Header;
      Print(L"  Manufacturer: %a\n", GetSmbiosString(Entry->Header, Rec->Manufacturer));
      Print(L"  Product Name: %a\n", GetSmbiosString(Entry->Header, Rec->ProductName));
      Print(L"  Version:      %a\n", GetSmbiosString(Entry->Header, Rec->Version));
      Print(L"  Serial Number:%a\n", GetSmbiosString(Entry->Header, Rec->SerialNumber));
      break;
    }

    case SMBIOS_TYPE_BIOS_INFORMATION: {
      SMBIOS_TABLE_TYPE0 *Rec = (SMBIOS_TABLE_TYPE0 *)Entry->Header;
      Print(L"  Vendor:       %a\n", GetSmbiosString(Entry->Header, Rec->Vendor));
      Print(L"  Version:      %a\n", GetSmbiosString(Entry->Header, Rec->BiosVersion));
      Print(L"  Release Date: %a\n", GetSmbiosString(Entry->Header, Rec->BiosReleaseDate));
      break;
    }

    // You can add more detailed decoders for other types here...

    default: {
      // Generic hex dump for types without a specific decoder
      UINT8 *Data = (UINT8 *)Entry->Header;
      Print(L"Raw data dump:\n  Ofs: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
      Print(L"  --------------------------------------------------\n");
      for (UINTN i = 0; i < Entry->Header->Length; i += 16) {
        Print(L"  %02Xh: ", i);
        for (UINTN j = 0; j < 16; j++) {
          if (i + j < Entry->Header->Length) {
            Print(L"%02X ", Data[i+j]);
          }
        }
        Print(L"\n");
      }
      break;
    }
  }

  Print(L"\nPress ESC to return...");
  do {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  } while (Key.ScanCode != SCAN_ESC);
}

/**
  Draws the list of all found SMBIOS tables.
*/
STATIC
VOID
DrawSmbiosList()
{
  UINTN BackgroundColor;
  CHAR16 HandleString[16];

  // Clear the screen
  gST->ConOut->ClearScreen(gST->ConOut);

  // Header
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
  Print(L"%-54s %-8s %s\n", L" SMBIOS Type", L"Handle", L"Length");

  // List items
  for (UINTN i = 0; i < mSmbiosCount; i++) {
    SMBIOS_ENTRY *E = &mSmbiosList[i];

    // Determine the background color for the entire row
    BackgroundColor = (i == mSmbiosSelected) ? EFI_GREEN : EFI_BLUE;

    // 1. Print the type number in WHITE
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, BackgroundColor));
    Print(L" (%03d) ", E->Header->Type);

    // 2. Print the type name in YELLOW with padding
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, BackgroundColor));
    // Total column width is 54. The number part "(XXX) " is 7 chars.
    // So we pad the name to 47 characters.
    Print(L"%-47s", GetSmbiosTypeName(E->Header->Type));

    // 3. Print the Handle and Length in WHITE (with alignment fix)
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, BackgroundColor));
    
    // Create a string for the handle to apply padding.
    UnicodeSPrint(HandleString, sizeof(HandleString), L"%04Xh", E->Handle);
    
    // Print the handle left-aligned in an 8-character field, then the length.
    Print(L"%-8s %04Xh\n", HandleString, E->Header->Length);
  }

  // Restore default colors
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

/**
  Main loop for navigating the SMBIOS list.
*/
STATIC
VOID
SmbiosMainLoop()
{
  EFI_INPUT_KEY Key;
  BOOLEAN ExitLoop = FALSE;

  mSmbiosSelected = 0;
  DrawSmbiosList();

  while(!ExitLoop) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    switch(Key.ScanCode) {
      case SCAN_UP:
        if (mSmbiosSelected > 0) {
          mSmbiosSelected--;
          DrawSmbiosList();
        }
        break;
      case SCAN_DOWN:
        if (mSmbiosSelected + 1 < mSmbiosCount) {
          mSmbiosSelected++;
          DrawSmbiosList();
        }
        break;
      case SCAN_ESC:
        ExitLoop = TRUE;
        break;
      case SCAN_NULL:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          ShowSmbiosRecordDetail(&mSmbiosList[mSmbiosSelected]);
          // Redraw the list after returning from detail view
          DrawSmbiosList();
        }
        break;
    }
  }
}


/**
  Main entry point for the SMBIOS feature.
*/
VOID
ReadSmbiosData(VOID)
{
  EFI_STATUS                  Status;
  EFI_SMBIOS_PROTOCOL        *Smbios;
  EFI_SMBIOS_HANDLE           TempHandle;
  EFI_SMBIOS_TABLE_HEADER    *TempRecord;
  UINTN                       Index;

  Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR(Status)) {
    Print(L"Could not locate SMBIOS protocol: %r\n", Status);
    gBS->Stall(2000000); // 2 seconds
    return;
  }

  // 1. First pass: Count the tables to allocate memory
  mSmbiosCount = 0;
  TempHandle = SMBIOS_HANDLE_PI_RESERVED;
  while(TRUE) {
    Status = Smbios->GetNext(Smbios, &TempHandle, NULL, &TempRecord, NULL);
    if (EFI_ERROR(Status)) break;
    mSmbiosCount++;
  }

  if (mSmbiosCount == 0) return;

  // 2. Allocate memory for our list
  mSmbiosList = AllocateZeroPool(mSmbiosCount * sizeof(SMBIOS_ENTRY));
  if (mSmbiosList == NULL) {
    Print(L"Error: Not enough memory for SMBIOS list.\n");
    gBS->Stall(2000000);
    return;
  }

  // 3. Second pass: Populate the list
  Index = 0;
  TempHandle = SMBIOS_HANDLE_PI_RESERVED;
  while(TRUE) {
    Status = Smbios->GetNext(Smbios, &TempHandle, NULL, &TempRecord, NULL);
    if (EFI_ERROR(Status)) break;
    mSmbiosList[Index].Handle = TempHandle;
    mSmbiosList[Index].Header = TempRecord;
    Index++;
  }

  // 4. Enter the interactive navigation loop
  SmbiosMainLoop();

  // 5. Clean up
  FreePool(mSmbiosList);
  mSmbiosList = NULL;
  mSmbiosCount = 0;
}

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
