## Overview

Going to create applications for ZTEX 1.15y Multi-FPGA boards that would communicate at USB 2.0 speed? Using ztex_inouttraffic as a starting point, you would save weeks, skip studying timing diagrams, bits in various registers and hangups.

## Contents

- C library to operate Ztex multi-FPGA boards at high-speed (up to 20-30 Mbytes/s) and corresponding firmware for USB device controller;
- example FPGA application (Verilog), example host software in C.

## Details explained

ztex.c - contains functions from original ztex.java library. Works with any firmware developed with Ztex SDK. Includes upload of firmware, bitstream, select individual fpga, reset given fpga to its pre-configuration state etc. Some functions such as write into onboard flash are not implemented - you can do that using FWLoader from Ztex SDK.

inouttraffic.c - contains functions for input/output at high-speed. Uses ztex.c. There must be inouttraffic.ihx firmware that implement those operations, and corresponding bitstream (built base on top-level HDL module ztex_inouttraffic.v). (*)

pkt_comm.c - functions and data structures used to communicate with fpgas in a sequence of application packets. Dependent on type, packet from the host goes into a pre-defined subsystem of fpga application. Similarily, data received from fpga aren't raw bytes with no start or end; pieces of data are organized as packets with properties such as ID, length, type, etc. and appear at application developer's hand as described in header files. This requires pkt_comm.v HDL module built on top of ztex_inouttraffic.v. (**)

device.c - contains top-level functions for application developer. That includes: search, detection and initialization of boards; read/write at high-speed using pkt_comm. Operates many boards with single function call.


(*) 'inouttraffic' explained.
This requires in-depth understanding of Ztex board and USB device controller. Briefly:
- USB device controller IC has embedded CPU (programmable in C). CPU can read/write controller's I/O pins individually or in groups of 8. Several dozens I/O pins are connected to pins of fpgas, all 4 fpga in parallel. USB packets can be handled by CPU which can read/write USB endpoint buffer and get/send data from/to fpgas. That would result in speed no more than 0.5-1 Mbyte/s. So this low-speed interface is to help establish a high-speed communication and for maintenance purposes.
- High-speed interface can be of several forms. Slave FIFO variant is used by 'inouttraffic'. It can transmit 16 bits in one direction every cycle without usage of CPU, resulting in 20-30 Mbytes/s. Several points were taken in consideration:
-- because I/O pins of all 4 fpga's are connected in parallel, only 1 fpga is active at given time. Dedicated CS signals are used to define which one is selected;
-- when fpga sends or receives data, host and controller wait until the end of transmission before setting other fpga to be selected;
-- caution is taken not to overflow fpga's input buffer. Device controller also has its internal buffers and data should not get stuck there;
-- host software must know the length of data fpga is going to transmit, must request for reading exactly that many.
One would ask - why so complex? Indeed, on a single-fpga Ztex board that's simple. Complexity is added by a requirement to select one fpga at a time while maintaining data integrity in a high-speed transmission.

(**) 'pkt_comm' explained.
API provided by 'inouttraffic' allows high-speed communication. On fpga side, there're I/O FIFOs and on the host side, there's a number of functions for operation. You can see it works in test.c.
Application developers often demand more, including:
- more abstract interface, without details of hardware or link layer so application can be ported to other device with different SDK;
- more abilities to manage incoming and outgoing data. The goal is accomplished with a concept of splitting all the data into application level packets.

## Development issues

Host software performs read/write operations with usb_bulk_transfer calls. That's blocking calls. So:
- if you have several boards on different USB busses, you have to address the issue to achive I/O performance.
- if you do some heavy computation on host CPU, and at same time you require high-speed communication to boards, that will require a separate thread or process to operate communication to boards. Alternatively, asynchronous USB transfer functions can be used.

## Miscellanous

- FPGA interconnect. Each FPGA has more than 200 I/O pins floating. Unluckily FPGAs aren't connected to each other with that unused pins.
- How to achive more performance over USB 2.0. You have to send/receive data in larger packets such as 16-64 Kbytes to achive performance equal to common hardware such as flash drive. Also USB transfers use CPU a lot, performance degrades if CPU is busy.
