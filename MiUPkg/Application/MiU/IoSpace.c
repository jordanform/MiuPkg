// IoSpace.c (New Version)

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include "IoSpace.h"

#define IO_BYTES_PER_LINE 16
#define IO_TOTAL_BYTES    256

/**
  Launch a 16x16 hex viewer of I/O space starting at Base.
  This function redraws the entire screen on each key press to achieve
  row and column highlighting, similar to ShowConfigSpace.
*/
STATIC
VOID
ShowIoSpace(
  IN UINT16 Base
  )
{
  UINT8         IoValues[IO_TOTAL_BYTES];
  UINTN         Selected = 0;
  BOOLEAN       ExitView = FALSE;
  EFI_INPUT_KEY Key;

  // Define a timer event to periodically refresh the display
  EFI_STATUS    Status;
  EFI_EVENT     TimerEvent;
  EFI_EVENT     WaitEvents[2];
  UINTN         Index;

  // 1. Create a timer event to trigger periodic updates
  Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
  if (EFI_ERROR(Status)) {
    Print(L"Could not create timer event: %r\n", Status);
    return;
  }

  // 2. Set the timer to trigger every second (10000000 nanoseconds)
  Status = gBS->SetTimer(TimerEvent, TimerPeriodic, 10000000);
  if (EFI_ERROR(Status)) {
    Print(L"Could not set timer: %r\n", Status);
    gBS->CloseEvent(TimerEvent);
    return;
  }

  // 3. Prepare to wait for key input and timer events
  WaitEvents[0] = gST->ConIn->WaitForKey;
  WaitEvents[1] = TimerEvent;

  // 4. Read all I/O values once to cache them
  gST->ConOut->EnableCursor(gST->ConOut, FALSE);
  for (UINTN i = 0; i < IO_TOTAL_BYTES; i++) {
    IoValues[i] = IoRead8(Base + (UINT16)i);
  }

  // 5. Main redraw and key processing loop
  while (!ExitView) {
    UINTN RowSel = Selected / IO_BYTES_PER_LINE;
    UINTN ColSel = Selected % IO_BYTES_PER_LINE;

    // Redraw header every time
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"IO Space   Start:%04x   End:%04x\n\n", Base, Base + IO_TOTAL_BYTES - 1);

    // Draw column labels with highlight
    Print(L"   "); // Indent for row labels
    for (UINTN c = 0; c < IO_BYTES_PER_LINE; c++) {
      if (c == ColSel) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      }
      Print(L" %02x", c);
    }
    Print(L"\n");

    // Draw the 16x16 data table
    for (UINTN Row = 0; Row < IO_TOTAL_BYTES / IO_BYTES_PER_LINE; Row++) {
      // Draw row label with highlight
      if (Row == RowSel) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
      } else {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
      }
      Print(L"%02x:", Row * IO_BYTES_PER_LINE);

      // Draw 16 bytes for this row
      for (UINTN Col = 0; Col < IO_BYTES_PER_LINE; Col++) {
        if (Row == RowSel && Col == ColSel) {
          // The selected cell
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_GREEN));
        } else if (Row == RowSel || Col == ColSel) {
          // Cells in the selected row or column
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
        } else {
          // All other cells
          gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
        }
        Print(L" %02x", IoValues[Row * IO_BYTES_PER_LINE + Col]);
      }
      Print(L"\n");
    }

    // Draw footer
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"\nUse left/right, up/down to move, ESC to return");

    // 6. Wait for either key input or timer event
    gBS->WaitForEvent(2, WaitEvents, &Index);

    // 7. Process the event that occurred
    if (Index == 0) {
      // Key input event
      gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
      switch (Key.ScanCode) {
        case SCAN_UP:    if (Selected >= IO_BYTES_PER_LINE) Selected -= IO_BYTES_PER_LINE; break;
        case SCAN_DOWN:  if (Selected + IO_BYTES_PER_LINE < IO_TOTAL_BYTES) Selected += IO_BYTES_PER_LINE; break;
        case SCAN_LEFT:  if ((Selected % IO_BYTES_PER_LINE) != 0) Selected--; break;
        case SCAN_RIGHT: if ((Selected % IO_BYTES_PER_LINE) != (IO_BYTES_PER_LINE - 1)) Selected++; break;
        case SCAN_ESC:   ExitView = TRUE; break;
      }
    } else if (Index == 1) {
      // Timer event - redraw the screen
      // Re-read all I/O values to ensure they are up-to-date
      for (UINTN i = 0; i < IO_TOTAL_BYTES; i++) {
        IoValues[i] = IoRead8(Base + (UINT16)i);
      }
      // Redraw the screen
    }
  }

  // End the timer event and close it
  gBS->SetTimer(TimerEvent, TimerCancel, 0);
  gBS->CloseEvent(TimerEvent);

  // Restore cursor and attributes before exiting
  gST->ConOut->EnableCursor(gST->ConOut, TRUE);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
}


/**
  Prompt the user for a starting I/O port address, then call ShowIoSpace().
*/
EFI_STATUS
ReadIoSpace(VOID)
{
  CHAR16         Buffer[5]   = L"";
  UINTN          Index       = 0;
  EFI_INPUT_KEY  Key;

  gST->ConOut->ClearScreen(gST->ConOut);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
  Print(L"IO Space   Start:____   End:____\n\n");
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(L"Type:IO Space Start: ");
  gST->ConOut->EnableCursor(gST->ConOut, TRUE);

  while (TRUE) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      break;
    } else if (Key.UnicodeChar == CHAR_BACKSPACE && Index > 0) {
      Index--; Buffer[Index] = L'\0';
      Print(L"\b \b"); // More robust backspace handling
    } else if (Index < 4 &&
        ((Key.UnicodeChar >= L'0' && Key.UnicodeChar <= L'9') ||
         (Key.UnicodeChar >= L'a' && Key.UnicodeChar <= L'f') ||
         (Key.UnicodeChar >= L'A' && Key.UnicodeChar <= L'F'))) {
      Buffer[Index++] = Key.UnicodeChar;
      Print(L"%c", Key.UnicodeChar);
    }
  }

  UINT16 Start = (UINT16)StrHexToUint64(Buffer);
  ShowIoSpace(Start);
  return EFI_SUCCESS;
}