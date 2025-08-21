#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/SmBios.h>
#include "Smbios.h"


// Globals for SMBIOS record list and navigation
STATIC SMBIOS_ENTRY *mSmbiosList = NULL;   // Array of SMBIOS_ENTRY structs
STATIC UINTN         mSmbiosCount = 0;     // Number of SMBIOS records found
STATIC UINTN         mSmbiosSelected = 0;  // Currently selected record index

/**
  Returns a human-readable name for a given SMBIOS type ID.
*/
STATIC CONST CHAR16* GetSmbiosTypeName(IN UINT8 Type) {
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
    case SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION:      return L"On Board Devices Information";
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
  Retrieves a string from an SMBIOS record.
  @param  SmbiosHeader  Pointer to the SMBIOS structure.
  @param  StringNumber  The 1-based index of the string to retrieve.
  @return               A pointer to the CHAR8 string, or a default string.
*/
STATIC CONST CHAR8* GetSmbiosString(IN SMBIOS_STRUCTURE *SmbiosHeader, IN SMBIOS_TABLE_STRING StringNumber) {
  UINTN  i;
  CHAR8 *String;
  String = (CHAR8 *)SmbiosHeader + SmbiosHeader->Length;
  if (StringNumber == 0) return "Not specified";
  for (i = 1; i < StringNumber; i++) {
    while (*String != 0) String++;
    String++;
  }
  if (*String == 0) return "Invalid String";
  return String;
}

/**
  Displays the detailed information for a single SMBIOS record.
  Shows decoded fields for known types, or a hex dump for unknown types.
  Waits for ESC key to return.
*/
STATIC VOID ShowSmbiosRecordDetail(IN SMBIOS_ENTRY *Entry) {
  EFI_INPUT_KEY Key;
  UINTN SavedAttr = gST->ConOut->Mode->Attribute;

  gST->ConOut->ClearScreen(gST->ConOut);

  //
  // Header line: white on red background
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
  Print(L"--- SMBIOS Record Detail ---\n");

  //
  // Back to main theme (blue background)
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

  //
  // "Type:" (white) + value (yellow)
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Type: ");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
  Print(L"%d (%s)\n", Entry->Header->Type, GetSmbiosTypeName(Entry->Header->Type));

  //
  // "Handle:" (white) + value (yellow)
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Handle: ");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
  Print(L"0x%04X\n", Entry->Handle);

  //
  // "Length:" (white) + value (yellow)
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Length: ");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
  Print(L"0x%02X\n\n", Entry->Header->Length);

  //
  // Decoded view for known types with the same color rule (title white, value yellow)
  //
  switch (Entry->Header->Type) {
    case SMBIOS_TYPE_SYSTEM_INFORMATION: {
      SMBIOS_TABLE_TYPE1 *Rec = (SMBIOS_TABLE_TYPE1 *)Entry->Header;

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Manufacturer: ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n", GetSmbiosString(Entry->Header, Rec->Manufacturer));

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Product Name: ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n", GetSmbiosString(Entry->Header, Rec->ProductName));

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Version:      ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n", GetSmbiosString(Entry->Header, Rec->Version));

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Serial Number:");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n\n", GetSmbiosString(Entry->Header, Rec->SerialNumber));
      break;
    }

    case SMBIOS_TYPE_BIOS_INFORMATION: {
      SMBIOS_TABLE_TYPE0 *Rec = (SMBIOS_TABLE_TYPE0 *)Entry->Header;

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Vendor:       ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n", GetSmbiosString(Entry->Header, Rec->Vendor));

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Version:      ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n", GetSmbiosString(Entry->Header, Rec->BiosVersion));

      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  Release Date: ");
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      Print(L"%a\n\n", GetSmbiosString(Entry->Header, Rec->BiosReleaseDate));
      break;
    }

    default:
      // keep behavior below (raw dump); known types will also show raw dump for consistency
      break;
  }

  //
  // Raw dump title and header (title white on blue; bytes lightgray on blue)
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Raw data dump:\n");
  Print(L"Ofs: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  Print(L"  --------------------------------------------------\n");

  {
    UINT8 *Data = (UINT8 *)Entry->Header;
    for (UINTN i = 0; i < Entry->Header->Length; i += 16) {
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L"  %02Xh: ", i);
      for (UINTN j = 0; j < 16; j++) {
        if (i + j < Entry->Header->Length) {
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
          Print(L"%02X ", Data[i + j]);
        }
      }
      Print(L"\n");
    }
  }

  //
  // Footer prompt
  //
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"\nPress ESC to return...");

  do {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  } while (Key.ScanCode != SCAN_ESC);

  gST->ConOut->SetAttribute(gST->ConOut, SavedAttr);
}

/**
  Prompt user to input 2-digit hexadecimal, return TRUE if successfully parsed;
  Return FALSE if user presses ESC to cancel.
  The result will be stored into OutType (0x00 ~ 0xFF).
*/
STATIC
BOOLEAN
PromptSmbiosTypeHex(OUT UINT8 *OutType)
{
  CHAR16        Buffer[3] = L"";  // 2 hex chars + NUL
  UINTN         Index     = 0;
  EFI_INPUT_KEY Key;

  gST->ConOut->ClearScreen(gST->ConOut);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
  Print(L"Jump to SMBIOS Type\n\n");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Enter Type (00-FF), then press Enter.  ESC to cancel.\n\n");
  Print(L"Type: 0x");
  gST->ConOut->EnableCursor(gST->ConOut, TRUE);

  while (TRUE) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      // Must have 1~2 digits to accept
      if (Index > 0) {
        UINTN val = StrHexToUintn(Buffer);
        if (val <= 0xFF) {
          *OutType = (UINT8)val;
          gST->ConOut->EnableCursor(gST->ConOut, FALSE);
          return TRUE;
        }
      }
      // Invalid input, keep waiting
    } else if (Key.ScanCode == SCAN_ESC) {
      gST->ConOut->EnableCursor(gST->ConOut, FALSE);
      return FALSE; // User canceled
    } else if (Key.UnicodeChar == CHAR_BACKSPACE && Index > 0) {
      Index--;
      Buffer[Index] = L'\0';
      Print(L"\b \b"); // Visual backspace
    } else if (Index < 2 &&
              ((Key.UnicodeChar >= L'0' && Key.UnicodeChar <= L'9') ||
               (Key.UnicodeChar >= L'a' && Key.UnicodeChar <= L'f') ||
               (Key.UnicodeChar >= L'A' && Key.UnicodeChar <= L'F'))) {
      Buffer[Index++] = Key.UnicodeChar;
      Print(L"%c", Key.UnicodeChar);
    }
  }
}

/**
  Searches for a specific SMBIOS type in the list.
  Returns the index of the first matching type, or -1 if not found.
*/
STATIC
INTN
FindSmbiosIndexByType(IN UINT8 Type)
{
  if (mSmbiosList == NULL || mSmbiosCount == 0) return -1;
  for (UINTN i = 0; i < mSmbiosCount; i++) {
    if (mSmbiosList[i].Header != NULL && mSmbiosList[i].Header->Type == Type) {
      return (INTN)i;
    }
  }
  return -1;
}

/**
  Draws the list of all found SMBIOS tables.
  Highlights the currently selected record.
*/
STATIC VOID DrawSmbiosList() {
  UINTN BackgroundColor;
  CHAR16 HandleString[16];

  // If no SMBIOS records are available, show a message and return
  if (mSmbiosList == NULL || mSmbiosCount == 0) {
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"No SMBIOS records found.\n");
    return;
  }
  
  // Clear the screen and set the header attributes
  gST->ConOut->ClearScreen(gST->ConOut);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));

  // Print the header
  Print(L"%-54s %-8s %s\n", L" SMBIOS Type", L"Handle", L"Length");

  // Print the list of SMBIOS entries
  for (UINTN i = 0; i < mSmbiosCount; i++) {
    SMBIOS_ENTRY *E = &mSmbiosList[i];
    BackgroundColor = (i == mSmbiosSelected) ? EFI_GREEN : EFI_BLUE;
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, BackgroundColor));
    Print(L" (%03d) ", E->Header->Type);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, BackgroundColor));
    Print(L"%-47s", GetSmbiosTypeName(E->Header->Type));
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, BackgroundColor));
    UnicodeSPrint(HandleString, sizeof(HandleString), L"%04Xh", E->Handle);
    Print(L"%-8s %04Xh\n", HandleString, E->Header->Length);
  }

  Print(L"\nHint: Press 't' to jump to a specific SMBIOS Type (00-FF)\n");
  
  // Restore default attribute
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
}

/**
  Main loop for navigating the SMBIOS list and viewing record details.
  Handles user input for navigation and selection.
*/
STATIC VOID SmbiosMainLoop() {

  // Main loop for SMBIOS record selection and display
  EFI_INPUT_KEY Key;
  BOOLEAN ExitLoop = FALSE;
  mSmbiosSelected = 0;

  // Draw the initial list of SMBIOS records
  DrawSmbiosList();

  // Event loop to handle user input
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
          DrawSmbiosList();
        }
        // Check for 't' key to jump to a specific type
        else if (Key.UnicodeChar == L't' || Key.UnicodeChar == L'T') {
          UINT8 Want;
          if (PromptSmbiosTypeHex(&Want)) {
            INTN idx = FindSmbiosIndexByType(Want);
            if (idx >= 0) {
              mSmbiosSelected = (UINTN)idx;
              // jump straight into detail view
              ShowSmbiosRecordDetail(&mSmbiosList[mSmbiosSelected]);
              // return to list after ESC in detail view
              DrawSmbiosList();
            } else {
              // Type not found, show message
              gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
              Print(L"\nType 0x%02x not found. Press any key...", Want);
              gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
              // Redraw the list after waiting
              DrawSmbiosList();
            }
          } else {
            // User canceled the type input
            DrawSmbiosList();
          }
        }
        break;
    }
  }
}

/**
  Main entry point for the SMBIOS feature.
  Enumerates SMBIOS tables, allocates memory, and enters navigation loop.
*/
VOID ReadSmbiosData(VOID) {
  EFI_STATUS                  Status;
  EFI_SMBIOS_PROTOCOL        *Smbios;
  EFI_SMBIOS_HANDLE           TempHandle;
  EFI_SMBIOS_TABLE_HEADER    *TempRecord;
  UINTN                       Index;

  // Locate the SMBIOS protocol
  Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR(Status)) {
    Print(L"Could not locate SMBIOS protocol: %r\n", Status);
    gBS->Stall(2000000);
    return;
  }

  // Initialize globals
  mSmbiosCount = 0;

  // First pass to count the number of SMBIOS records
  TempHandle = SMBIOS_HANDLE_PI_RESERVED;
  while(TRUE) {
    Status = Smbios->GetNext(Smbios, &TempHandle, NULL, &TempRecord, NULL);
    if (EFI_ERROR(Status)) break;
    mSmbiosCount++;
  }

  // If no records found, exit early
  if (mSmbiosCount == 0) return;

  // Allocate memory for the SMBIOS entries
  mSmbiosList = AllocateZeroPool(mSmbiosCount * sizeof(SMBIOS_ENTRY));
  if (mSmbiosList == NULL) {
    Print(L"Error: Not enough memory for SMBIOS list.\n");
    gBS->Stall(2000000);
    return;
  }

  // Second pass to actually read the SMBIOS records into the list
  Index = 0;
  TempHandle = SMBIOS_HANDLE_PI_RESERVED;
  while(TRUE) {
    Status = Smbios->GetNext(Smbios, &TempHandle, NULL, &TempRecord, NULL);
    if (EFI_ERROR(Status)) break;
    mSmbiosList[Index].Handle = TempHandle;
    mSmbiosList[Index].Header = TempRecord;
    Index++;
  }

  // Check if we read the expected number of records
  if (Index != mSmbiosCount) {
    Print(L"Warning: Mismatch in SMBIOS count (%d vs %d)\n", Index, mSmbiosCount);
  }

  // Start the main loop for SMBIOS record navigation
  SmbiosMainLoop();

  // Free the allocated memory for SMBIOS entries
  FreePool(mSmbiosList);
  mSmbiosList = NULL;
  mSmbiosCount = 0;
}
