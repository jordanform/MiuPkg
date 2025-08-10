#ifndef FILE_HELPER_H_
#define FILE_HELPER_H_

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>

/**
  Save a buffer of bytes to a file on the first available simple file system.

  @param[in]  ImageHandle  The EFI image handle.
  @param[in]  FileName     A UTF-16 string (e.g. L"dump.bin").
  @param[in]  Buffer       Pointer to the byte buffer to write.
  @param[in]  BufferSize   Size of the buffer, in bytes.

  @retval EFI_SUCCESS      The file was written successfully.
  @retval others           An EFI_STATUS error code if the operation failed.
**/
EFI_STATUS
SaveBytesToFile (
  IN EFI_HANDLE  ImageHandle,
  IN CHAR16      *FileName,
  IN UINT8       *Buffer,
  IN UINTN       BufferSize
  );

#endif // FILE_HELPER_H_
