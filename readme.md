# MiU (My Information UEFI) Package

This package contains a UEFI application called `MiU` that provides a text-based user interface to explore various system information available in the UEFI environment.

## Features

*   **PCI Device Enumeration:** Lists all PCI devices found in the system. You can select a device to view its 256-byte configuration space in a hex dump format.
*   **SMBIOS Record Viewer:** Displays all SMBIOS tables, allowing you to inspect the details of each record.
*   **ACPI Table Viewer:** Lists all ACPI tables found from the RSDT and XSDT.
*   **UEFI Variable Viewer:** Lists all UEFI variables and allows you to view their raw data.
*   **Interactive TUI:** The application uses a colored text-based interface for easy navigation.

## How to Use

1.  Build the `MiUPkg` using the EDK2 build system. The output will be `MiU.efi`.
2.  Launch the UEFI shell and run `MiU.efi`.
3.  The application will start by showing the PCI device list.
4.  Use the following hotkeys to switch between different views:
    *   `Alt+1`: PCI Devices
    *   `Alt+2`: SMBIOS Records
    *   `Alt+3`: ACPI Tables
    *   `Alt+4`: UEFI Variables
5.  Use the arrow keys to navigate, `Enter` to select, and `ESC` to go back or quit.
6.  Press 'h' at any time to see a help popup with the list of hotkeys.

## Building

The application can be built using the standard EDK2 build process. The main platform description file is `MiUPkg/MiUPkg.dsc`.
