#include "FileHelper.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Library/DevicePathLib.h>

EFI_STATUS
SaveBytesToFile (
  IN EFI_HANDLE  ImageHandle,
  IN CHAR16      *FileName,
  IN UINT8       *Buffer,
  IN UINTN       BufferSize
  )
{
  EFI_STATUS                        Status;
  EFI_LOADED_IMAGE_PROTOCOL        *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs;
  EFI_FILE_PROTOCOL                *RootDir;
  EFI_FILE_PROTOCOL                *FileHandle;
  UINTN                             Size = BufferSize;

  // 1) Get the Loaded Image Protocol for this image
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID**)&LoadedImage
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // 2) Get the Simple File System Protocol from the device handle
  Status = gBS->HandleProtocol (
                  LoadedImage->DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID**)&SimpleFs
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // 3) Open the volume (root directory)
  Status = SimpleFs->OpenVolume(SimpleFs, &RootDir);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // 4) Create or open the target file
  Status = RootDir->Open(
                     RootDir,
                     &FileHandle,
                     FileName,
                     EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ,
                     0
                     );
  if (EFI_ERROR(Status)) {
    RootDir->Close(RootDir);
    return Status;
  }

  // 5) Write the buffer to the file
  Status = FileHandle->Write(FileHandle, &Size, Buffer);
  if (EFI_ERROR(Status)) {
    FileHandle->Close(FileHandle);
    RootDir->Close(RootDir);
    return Status;
  }

  // 6) Close file and directory handles
  FileHandle->Close(FileHandle);
  RootDir->Close(RootDir);
  return EFI_SUCCESS;
}
