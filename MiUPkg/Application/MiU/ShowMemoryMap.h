#ifndef SHOW_MEMORY_MAP_H
#define SHOW_MEMORY_MAP_H

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

// Maximum number of memory descriptors supported (adjust as needed)
#define MEMORY_MAP_MAX_DESCRIPTORS 128

/**
  Display the system memory map with paging and scrolling support.
**/
VOID
ShowMemoryMap(VOID);

#endif // SHOW_MEMORY_MAP_H