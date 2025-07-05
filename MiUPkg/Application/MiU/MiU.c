#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
//#include <Library/ShellLib.h>
#include <Protocol/PciIo.h>

// Remove redundant extern, ensure correct include for GUID definition

#define MAX_DEVICES  256

/**
  Data structure describing one PCI device entry in the table.
*/
typedef struct {
  EFI_HANDLE             Handle;
  EFI_PCI_IO_PROTOCOL   *PciIo;
  UINT8                  Segment;
  UINT8                  Bus;
  UINT8                  Dev;
  UINT8                  Func;
} PCI_ENTRY;

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
STATIC
EFI_STATUS
EnumeratePci (VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuf;
  UINTN       HandleCount;
  UINTN       Idx;

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiPciIoProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuf);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mPciList = AllocateZeroPool (HandleCount * sizeof (PCI_ENTRY));
  if (mPciList == NULL) {
    FreePool (HandleBuf);
    return EFI_OUT_OF_RESOURCES;
  }


  for (Idx = 0; Idx < HandleCount; ++Idx) {
    PCI_ENTRY *Entry = &mPciList[Idx];
    Entry->Handle   = HandleBuf[Idx];

    Status = gBS->HandleProtocol (HandleBuf[Idx],
                                  &gEfiPciIoProtocolGuid,
                                  (VOID **)&Entry->PciIo);
    if (EFI_ERROR(Status) || Entry->PciIo == NULL) {
      Print(L"[ERROR] HandleProtocol failed for handle %u: %r\n", Idx, Status);
      continue;
    }

    Status = Entry->PciIo->GetLocation (Entry->PciIo,
                                        (UINTN *)&Entry->Segment,
                                        (UINTN *)&Entry->Bus,
                                        (UINTN *)&Entry->Dev,
                                        (UINTN *)&Entry->Func);
    if (EFI_ERROR(Status)) {
      Print(L"[ERROR] GetLocation failed for handle %u: %r\n", Idx, Status);
      continue;
    }
  }

  mPciCount = HandleCount;
  FreePool (HandleBuf);
  return EFI_SUCCESS;
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
  ConOut->ClearScreen (ConOut);

  for (UINTN Idx = 0; Idx < mPciCount; ++Idx) {
    BOOLEAN Sel = (Idx == mSelected);
    ConOut->SetAttribute (ConOut,
      Sel ? EFI_TEXT_ATTR (EFI_BLACK, EFI_LIGHTGRAY)  // Highlight
          : EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLUE)); // Normal

    Print (L"%02x:%02x.%x\n",
           mPciList[Idx].Bus,
           mPciList[Idx].Dev,
           mPciList[Idx].Func);
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
ShowConfigSpace (PCI_ENTRY *Entry)
{
  UINT8  Data[256];
  UINT32 Offset;

  // Read DWORD-by-DWORD for simplicity (ASCII hyphen)
  for (Offset = 0; Offset < 256; Offset += 4) {
    Entry->PciIo->Pci.Read (Entry->PciIo, EfiPciIoWidthUint32, Offset, 1, &Data[Offset]);
  }

  gST->ConOut->ClearScreen (gST->ConOut);

  for (Offset = 0; Offset < 256; Offset += 16) {
    Print (L"%02x: ", Offset);
    for (UINTN i = 0; i < 16; ++i) {
      Print (L"%02x ", Data[Offset + i]);
    }
    Print (L"\n");
  }

  Print (L"\nPress any key to return...");
  EFI_INPUT_KEY Key;
  gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
}

/**
  Main loop: use arrow keys to move the highlight, ENTER to view config space,
  ESC to quit the application.
*/
STATIC
VOID
MainLoop (VOID)
{
  EFI_INPUT_KEY Key;
  while (TRUE) {
    DrawDeviceList ();

    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    switch (Key.ScanCode) {
      case SCAN_UP:
        if (mSelected > 0) {
          --mSelected;
        }
        break;
      case SCAN_DOWN:
        if (mSelected + 1 < mPciCount) {
          ++mSelected;
        }
        break;
      case SCAN_ESC:
        return;                     // Exit program
      case SCAN_NULL:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          ShowConfigSpace (&mPciList[mSelected]);
        }
        break;
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
  EFI_STATUS Status = EnumeratePci ();
  if (EFI_ERROR (Status)) {
    Print (L"PCI enumeration failed: %r\n", Status);
    return Status;
  }

  MainLoop ();
  return EFI_SUCCESS;
}
