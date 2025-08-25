#ifndef __SHOW_BOOT_OPTION_H__
#define __SHOW_BOOT_OPTION_H__

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Display all Boot#### variables in the system.

  @retval EFI_SUCCESS     Successfully displayed all boot options.
  @retval Others          Failed to get or display boot options.
**/
EFI_STATUS
EFIAPI
ShowBootOptions (
  VOID
  );

#endif
