
Instructions on creation of FPGA configuration ("bitstream").
Used ISE Design Suite version 14.5 from Xilinx Design Tools.

- Run Project Navigator, create new project
- add all *.v, *.vh, *.ucf files (except for files from *-experimental folders)
- File "definitions.vh" (Right click) -> "Source Properties" -> "Include as Global File in Compile List"
- File "inouttraffic.v" (Right click) -> "Set as Top Module"
- Run "Implement Top Module". That would result in an error because it requires to add cores from Xilinx IP Coregen. It would display files and lines where that cores were instantiated. Near each one, there's a comment with details on creation of the core.
- Create abovementioned cores with IP Core Generator
- (Optionally) In "Design Goals & Strategies" add a strategy file inouttraffic.xds (will increase build time)
- "Generate Programming File"

There's already built bitstream file (fpga/inouttraffic.bit).

===================================================

Building host software.
It worked with following host software versions:

- Windows 7 (32 bit)
- CYGWIN_NT-6.1 2.4.1(0.293/5/3) 2016-01-24
- gcc (GCC) 5.3.0
- make (GNU Make 4.2.1)
- libusb-1.0

It requires a driver to access the device on Windows. Used to install WinUSB driver with the help from Zadig 2.2.

====================================================

Creation of firmware for USB device controller.
It might appear that 'inouttraffic' firmware already has all the required functions and application developer would not need that.

Assuming ZTEX 1.15y board is connected, the developer is familiar with Ztex SDK and got tests from Ztex SDK working.

- Get, unpack ZTEX SDK
- Install SDCC (Small Devices C Compiler) as recommended in ZTEX SDK manual
- Create ztex/examples/usb-fpga-1.15y/inouttraffic folder. Inside the folder:
-- copy files from examples/usb-fpga-1.15y/intraffic
-- place inouttraffic.c there
-- edit Makefile replacing 'intraffic' with 'inouttraffic'
- make

Versions used:
- ztex-140813b
- SDCC 3.6.0 #9615 (MINGW32)

There's already a built firmware file (run/ztex/inouttraffic.ihx).
