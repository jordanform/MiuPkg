#include "ShowMemoryMap.h"

/**
  Structure to hold memory map information.
**/
typedef struct {
  EFI_MEMORY_DESCRIPTOR *Map;            // Pointer to memory descriptor buffer
  UINTN                MapSize;         // Size of the buffer in bytes
  UINTN                DescriptorSize;  // Size of each descriptor
  UINT32               DescriptorVersion;
  UINTN                DescriptorCount; // Number of descriptors
} MEMORY_MAP;

/**
  Retrieve the memory map into a dynamically allocated buffer.
  Caller must free the buffer using FreePool.
  
  @param[in,out] MemMap   Pointer to MEMORY_MAP structure.
  @retval EFI_SUCCESS     Memory map retrieved successfully.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
**/
STATIC
EFI_STATUS
GetMemoryMapBuffer(
  IN OUT MEMORY_MAP *MemMap
  )
{
  EFI_STATUS Status;
  UINTN    MapKey;

  // First call to determine required buffer size
  MemMap->MapSize = 0;
  Status = gBS->GetMemoryMap(
             &MemMap->MapSize,
             MemMap->Map,
             &MapKey,
             &MemMap->DescriptorSize,
             &MemMap->DescriptorVersion
             );
    
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // Allocate buffer with extra space for safety
    MemMap->Map = AllocatePool(MemMap->MapSize + MemMap->DescriptorSize * 2);
    if (MemMap->Map == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    // Retrieve the memory map into allocated buffer
    Status = gBS->GetMemoryMap(
               &MemMap->MapSize,
               MemMap->Map,
               &MapKey,
               &MemMap->DescriptorSize,
               &MemMap->DescriptorVersion
               );
  }
  return Status;
}

//----------------------------------------------------------------------
// Map EFI_MEMORY_TYPE values to text
//----------------------------------------------------------------------

STATIC
CONST CHAR16 *mMemoryTypeName[] = {
  L"EfiReservedMemoryType",     // 0
  L"EfiLoaderCode",             // 1
  L"EfiLoaderData",             // 2
  L"EfiBootServicesCode",       // 3
  L"EfiBootServicesData",       // 4
  L"EfiRuntimeServicesCode",    // 5
  L"EfiRuntimeServicesData",    // 6
  L"EfiConventionalMemory",     // 7
  L"EfiUnusableMemory",         // 8
  L"EfiACPIReclaimMemory",      // 9
  L"EfiACPIMemoryNVS",          // 10
  L"EfiMemoryMappedIO",         // 11
  L"EfiMemoryMappedIOPortSpace",// 12
  L"EfiPalCode",                // 13
  L"EfiPersistentMemory"        // 14 (if defined in your headers)
};

#define MEMORY_TYPE_NAME_COUNT (sizeof(mMemoryTypeName)/sizeof(mMemoryTypeName[0]))

/**
  Print a single memory descriptor line.
  
  @param[in] Desc   Pointer to EFI_MEMORY_DESCRIPTOR to print.
**/
STATIC
VOID
DrawMemoryDescriptor(
  IN EFI_MEMORY_DESCRIPTOR *Desc
  )
{
  CONST CHAR16 *TypeString;

  // pick a safe string
  if (Desc->Type < MEMORY_TYPE_NAME_COUNT) {
    TypeString = mMemoryTypeName[Desc->Type];
  } else {
    TypeString = L"UnknownType";
  }

  // set colors
  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));

  // now print the text name instead of the raw number
  Print(
    L"%4u %-22s 0x%012lx 0x%04lx  %s\n",
    (UINT32)Desc->Type,
    TypeString,
    Desc->PhysicalStart,
    Desc->NumberOfPages,
    (Desc->Attribute & EFI_MEMORY_WB) ? L"WB" : L"--"
    );
}

/**
  Main entry point: display the memory map with paging and scrolling.
**/
VOID
ShowMemoryMap(VOID)
{
  EFI_STATUS       Status;
  MEMORY_MAP       MemMap = {0};
  UINTN            Index;
  UINTN            PageSize = 10;      // Number of descriptors per page
  UINTN            CurrentPage = 0;
  UINTN            PageCount;
  EFI_INPUT_KEY    Key;

  // Retrieve the full memory map
  Status = GetMemoryMapBuffer(&MemMap);
  if (EFI_ERROR(Status)) {
    Print(L"GetMemoryMap failed: %r\n", Status);
    return;
  }

  // Calculate total descriptors and pages
  MemMap.DescriptorCount = MemMap.MapSize / MemMap.DescriptorSize;
  PageCount = (MemMap.DescriptorCount + PageSize - 1) / PageSize;

  // Display loop
  while (TRUE) {
    // Clear the console
    gST->ConOut->ClearScreen(gST->ConOut);

    // Print header line with white text on red background
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_RED));
    Print(L"%4s %-22s %14s %6s  %s\n",
        L"Idx",            
        L"Type",           
        L"PhysicalStart",  
        L"Pages",          
        L"Attr"            
    );

    // Print descriptors on the current page
    for (Index = CurrentPage * PageSize;
         Index < MemMap.DescriptorCount && Index < (CurrentPage + 1) * PageSize;
         Index++) {
      EFI_MEMORY_DESCRIPTOR *Desc =
        (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemMap.Map + Index * MemMap.DescriptorSize);
      DrawMemoryDescriptor(Desc);
    }

    // Print footer with page information and controls
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    Print(L"Page %u/%u  Up/Down Scroll  ESC: Exit\n",
          CurrentPage + 1, PageCount);

    // Wait for user input
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    // Handle scrolling keys
    if (Key.ScanCode == SCAN_ESC) {
      break;
    } else if (Key.ScanCode == SCAN_UP) {
      if (CurrentPage > 0) {
        CurrentPage--;
      }
    } else if (Key.ScanCode == SCAN_DOWN) {
      if (CurrentPage + 1 < PageCount) {
        CurrentPage++;
      }
    } else if (Key.ScanCode == SCAN_PAGE_UP) {
      if (CurrentPage > 0) {
        CurrentPage--;
      }
    } else if (Key.ScanCode == SCAN_PAGE_DOWN) {
      if (CurrentPage + 1 < PageCount) {
        CurrentPage++;
      }
    }
  }

  // Free allocated buffer
  FreePool(MemMap.Map);
}