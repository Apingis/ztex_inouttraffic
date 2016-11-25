`timescale 1ns / 1ps

// clocks-ifclk.v
//
// Uses only input clock; DCMs and PLLs unused.
// Requires ifclk.ucf and hs_io_v2-ifclk.v

// **********************************************************************
//
// Input clocks:
// * IFCLK_IN 48 MHz.
// * FXCLK_IN 48 MHz.
//
// Output:
// * IFCLK - equal to IFCLK_IN, some phase backshift
// * other clocks
//
// **********************************************************************


module clocks #(
	parameter WORD_GEN_FREQ = 234,
	parameter PKT_COMM_FREQ = 174,
	parameter CORE_FREQ = 216,
	parameter CMP_FREQ = 156
	)(
	input IFCLK_IN,
	input FXCLK_IN,
	
	output IFCLK,
	output WORD_GEN_CLK,
	output PKT_COMM_CLK,
	output CORE_CLK,
	output CMP_CLK
	);


	// ********************************************************************************
	//
	// Attention developer!
	//
	// * On ZTEX 1.15y board, clocks coming from USB device controller do bypass a CPLD.
	// That's unknown why. To get Slave FIFO working, that requires clock phase backshift
	// (DCM can do) or equal measure. That might be the placement of input registers deep
	// into FPGA fabric or usage of IDELAY components.
	//
	// * If several DCMs and/or PLLs are used and their placement is not manually defined,
	// tools (ISE 14.5) place them randomly without a respect to dedicated lines.
	// That results in a usage of general routing for clocks, that in turn can
	// result in an unroutable condition if it's full of wires.
	//
	// * When tools notice derived clocks, they mess up with timing at Place and Route stage.
	//
	// ********************************************************************************



	// ****************************************************************************
	//
	// Spartan-6 Clocking Resources (Xilinx UG382) is anything but straightforward.
	//
	// ****************************************************************************

	// Tasks:
	// - generate a number of clocks for various parts of application
	// - don't use general routing for clocks
	// - define frequencies in MHz, not in magic units
	// - don't define derived clocks, 1 constraint should apply only to 1 clock.

	
	// IFCLK_IN and FXCLK_IN are located near each other.
	// There's some I/O clocking region there.
	// Limited number of dedicated routes from that region to CMTs are available.
	//
	// Each input clock can go to up to 2 CMTs, one of them must be
	// in the top half of fpga and other one must be in the bottom half.
	//
	// CMTs are numbered 0 to 5 from bottom to top.


	// Input clock goes directly into BUFG
	// (was: input clock goes to CMT, used to feed DCM)
	BUFG BUFG_0(
		.I(IFCLK_IN),
		.O(IFCLK)
	);

	// For simplicity all other clocks are set equal to IFCLK
	assign WORD_GEN_CLK = IFCLK;
	assign PKT_COMM_CLK = IFCLK;
	assign CORE_CLK = IFCLK;
	assign CMP_CLK = IFCLK;
	
	// As a result, on-chip frequency generation is not used.

	// To build:
	// - remove clocks.ucf
	// - add ifclk.ucf
	// - remove hs_io_v2.v, add hs_io_v2-ifclk.v
	

endmodule
