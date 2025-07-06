#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
//#include <Library/ShellLib.h>
#include <Protocol/PciIo.h>

// Remove redundant extern, ensure correct include for GUID definition

#define MAX_DEVICES  256

// Simple PCI ID to name mapping table
typedef struct {
  UINT16     VendorId;
  UINT16     DeviceId;
  CHAR16    *Name;
} PCI_NAME_ENTRY;

STATIC PCI_NAME_ENTRY mPciNameTable[] = {
  { 0x8086, 0x1237, L"Intel 82441FX MARS Pentium Pro to PCI" },
  { 0x8086, 0x7000, L"Intel 82371SB ISA bridge" },
  { 0x8086, 0x7010, L"Intel Triton PIIX3 IDE controller" },
  { 0x8086, 0x7113, L"Intel 82371AB Power Management Bridge" },
  { 0x1234, 0x1111, L"Sample Device" },
  { 0,      0,      NULL }
};

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
  UINT16                 VendorId;
  UINT16                 DeviceId;
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

    // Read Vendor and Device IDs
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x00,
      1,
      &Entry->VendorId
    );
    Entry->PciIo->Pci.Read(
      Entry->PciIo,
      EfiPciIoWidthUint16,
      0x02,
      1,
      &Entry->DeviceId
    );    
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

  // Draw header row
  ConOut->ClearScreen(ConOut);
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
  Print(
    L"%-8s %-22s %-15s\n",
    L"Name",
    L"Vendor:Device",
    L"Seg/Bus:Dev:Fun"
    );
  ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

  // Each device
  for (UINTN Index = 0; Index < mPciCount; Index++) {
    PCI_ENTRY *E = &mPciList[Index];
    BOOLEAN    Sel = (Index == mSelected);
    if (Sel) {
      ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGREEN));
    }
    // Device name
    CONST CHAR16 *Name = GetPciDeviceName(E->VendorId, E->DeviceId);
    // Print: Location+Name, Vendor:Device, Seg/Bus:Dev:Fun
    Print(
      L"%02x:%02x.%x %-30s %04x:%04x %02x/%02x:%02x:%x\n",
      E->Bus,
      E->Dev,
      E->Func,
      Name,
      E->VendorId,
      E->DeviceId,
      0,        // Segment (always 0 in most systems)
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
  UINT8 Data[256];

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

  gST->ConOut->ClearScreen(gST->ConOut);

  // Print selected device header
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
  Print(
    L"Device:%02x:%02x.%x   VID:DID = %04x:%04x\n\n",
    Entry->Bus,
    Entry->Dev,
    Entry->Func,
    Entry->VendorId,
    Entry->DeviceId
  );
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

  // Dump config space rows
  for (UINT32 Offset = 0; Offset < 256; Offset += 16) {
    Print(L"%02x: ", Offset);
    for (UINTN i = 0; i < 16; i++) {
      Print(L"%02x ", Data[Offset + i]);
    }
    Print(L"\n");
  }

  // Wait for ESC key to return
  Print(L"\nPress ESC to return...");
  EFI_INPUT_KEY Key;
  do {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  } while (Key.ScanCode != SCAN_ESC);
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
