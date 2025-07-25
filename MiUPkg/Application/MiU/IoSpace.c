// IoSpace.c

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>          // IoRead8()
#include <Library/BaseLib.h>        // StrHexToUint64()
#include <Library/PrintLib.h>
#include "IoSpace.h"

#define IO_BYTES_PER_LINE 16
#define IO_TOTAL_BYTES    256

// Screen origin for the table (column, row)
#define TABLE_X0 3
#define TABLE_Y0 2

// Draw a single cell in the I/O table
STATIC
VOID
DrawCell(
  IN UINTN   Idx,
  IN UINT8   Value,
  IN BOOLEAN Highlight
  )
{
  UINTN row = Idx / IO_BYTES_PER_LINE;
  UINTN col = Idx % IO_BYTES_PER_LINE;
  UINTN x   = TABLE_X0 + 3 + col * 3;
  UINTN y   = TABLE_Y0 + 1 + row;

  gST->ConOut->SetCursorPosition(gST->ConOut, x, y);
  if (Highlight) {
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGREEN));
  } else {
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  }
  Print(L" %02x", Value);
}

/**
  Draw header and full 16x16 I/O table once.
  Cache values into IoValues[].
*/
STATIC
VOID
DrawIoTable(
  IN UINT16 Base,
  OUT UINT8 IoValues[IO_TOTAL_BYTES]
  )
{
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;

  ConOut->ClearScreen(ConOut);
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"IO Space   Start:%04x   End:%04x\n", Base, Base + IO_TOTAL_BYTES - 1);

  // Column labels
  ConOut->SetCursorPosition(ConOut, TABLE_X0, TABLE_Y0);
  Print(L"   ");
  for (UINTN c = 0; c < IO_BYTES_PER_LINE; c++) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
    Print(L" %02x", c);
  }

  // Data rows
  for (UINTN row = 0; row < IO_TOTAL_BYTES / IO_BYTES_PER_LINE; row++) {
    UINTN rowBase = row * IO_BYTES_PER_LINE;
    UINTN y = TABLE_Y0 + 1 + row;
    // Row label
    ConOut->SetCursorPosition(ConOut, TABLE_X0, y);
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
    Print(L"%02x ", rowBase);

    // Bytes
    for (UINTN col = 0; col < IO_BYTES_PER_LINE; col++) {
      UINTN idx = rowBase + col;
      UINT8 v = IoRead8(Base + (UINT16)idx);
      IoValues[idx] = v;
      ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      Print(L" %02x", v);
    }
  }

  // Footer instructions
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  ConOut->SetCursorPosition(ConOut, TABLE_X0, TABLE_Y0 + IO_BYTES_PER_LINE + 2);
  Print(L"Use left/right, up/down to move, ESC to return");
}

/**
  Launch a 16x16 hex viewer of I/O space starting at Base; press ESC to exit.
  Only update the highlight cells on arrow key moves to reduce flicker.
*/
STATIC
VOID
ShowIoSpace(
  IN UINT16 Base
  )
{
  UINT8         IoValues[IO_TOTAL_BYTES];
  UINTN         Selected = 0;
  UINTN         PrevSel;
  BOOLEAN       ExitView = FALSE;
  EFI_INPUT_KEY Key;

  // Initialize the I/O values
  gST->ConOut->EnableCursor(gST->ConOut, FALSE);

  // 1) Draw the initial table and cache values
  DrawIoTable(Base, IoValues);

  // 2) Highlight initial cell
  DrawCell(Selected, IoValues[Selected], TRUE);

  // 3) Key loop: only update the highlight cells
  while (!ExitView) {
    // Wait for key without using the Ex protocol for simplicity in this view
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.ScanCode == SCAN_ESC) {
      ExitView = TRUE;
      continue;
    }

    PrevSel = Selected;
    switch (Key.ScanCode) {
      case SCAN_UP:
        if (Selected >= IO_BYTES_PER_LINE) Selected -= IO_BYTES_PER_LINE;
        break;
      case SCAN_DOWN:
        if (Selected + IO_BYTES_PER_LINE < IO_TOTAL_BYTES) Selected += IO_BYTES_PER_LINE;
        break;
      case SCAN_LEFT:
        if ((Selected & 0x0F) != 0) Selected--;
        break;
      case SCAN_RIGHT:
        if ((Selected & 0x0F) != 0x0F) Selected++;
        break;
      default:
        // Ignore other keys
        continue;
    }

    // 4) Clear previous highlight
    DrawCell(PrevSel, IoValues[PrevSel], FALSE);

    // 5) Highlight new cell
    DrawCell(Selected, IoValues[Selected], TRUE);

    // Enable cursor visibility at the end of the loop
    gST->ConOut->EnableCursor(gST->ConOut, TRUE);
  }
}


/**
  Prompt the user for a starting I/O port address (up to 4 hex digits),
  convert it to an integer via StrHexToUint64(), and then call ShowIoSpace().
*/
EFI_STATUS
ReadIoSpace(VOID)
{
  CHAR16         Buffer[5]   = L"";
  UINTN          Index       = 0;
  EFI_INPUT_KEY  Key;

  // Clear screen and display input prompt
  gST->ConOut->ClearScreen(gST->ConOut);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
  Print(L"IO Space   Start:____   End:____\n\n");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Type:IO Space Start: ");

  // Read hex input
  while (TRUE) {
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (Index < 4 &&
        ((Key.UnicodeChar >= L'0' && Key.UnicodeChar <= L'9') ||
         (Key.UnicodeChar >= L'a' && Key.UnicodeChar <= L'f') ||
         (Key.UnicodeChar >= L'A' && Key.UnicodeChar <= L'F'))) {
      Buffer[Index++] = Key.UnicodeChar;
      Print(L"%c", Key.UnicodeChar);
    } else if (Key.UnicodeChar == CHAR_BACKSPACE && Index > 0) {
      Index--; Buffer[Index] = L'\0';
      gST->ConOut->SetCursorPosition(
        gST->ConOut,
        gST->ConOut->Mode->CursorColumn - 1,
        gST->ConOut->Mode->CursorRow);
      Print(L" ");
      gST->ConOut->SetCursorPosition(
        gST->ConOut,
        gST->ConOut->Mode->CursorColumn - 1,
        gST->ConOut->Mode->CursorRow);
    } else if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      break;
    }
  }

  // Convert input and launch viewer
  UINT16 Start = (UINT16)StrHexToUint64(Buffer);
  ShowIoSpace(Start);
  return EFI_SUCCESS;
}
