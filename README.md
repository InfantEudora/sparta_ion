Open Source Sparta ION
=============
There's a bunch of (Dutch) blog posts that go with this: (http://infant.tweakblogs.net/blog/cat/2875)

This repository contains the following:

 folder       |  descr
--------------|-------------------------------------------------------------
 eagle        | All relevant eagle files, BOM lists.
 ion_firmware | AVR Studio projects for the different kinds of firmware.
 ion_software | Software for testing, or uploading hex files.
 lib          | Files used in either firmware or software.
 lib_ion      | Files used for Sparta ION specific code.

All files in this repository are released under GNU GPLv3 (https://www.gnu.org/copyleft/gpl.html)

Software used:
----------------
 - Atmel Studio 6.1.2440 - BETA (http://www.atmel.com/microsite/atmel_studio6/)
 - GCC 4.9.2
 - Windows: Cygwin(https://cygwin.com/install.html)
 - Eagle 6.1.0 (http://www.cadsoftusa.com/)

Firmware:
----------------

The firmware running your bike's motor, consists of three parts:
 - A bootloader, started on powerup.
 - The actual firmware, called application executed by the bootloader.
 - Configuration memory, stored in ROM.

The bootloader uses two blocks to identify the harware it's running on, and what firmware should be loaded.
The firware uses one block to read/write settings, which are currently the strain gauge calibration.

If you wan't to build a control PCB, use the latest revision of 3phasecntrl.

New Hardare Revision Wishlist:
 - Crystal
 - ESD protection on HALL input. (REV2)

