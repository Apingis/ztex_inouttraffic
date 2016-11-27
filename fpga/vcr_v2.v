`timescale 1ns / 1ps

//***********************************************************
//
// vcr_v2: uses only 9 lines (was 11)
// excluded:
// PA7 - used by bitstream upload
// PA1 - used by bitstream upload, other problems
//
//
// Vendor Command / Vendor Request feature
// Commands or requests are sent to EZ-USB via USB Endpoint 0
// and handled by EZ-USB processor.
//
// There's up to 256 VCR addresses (subsystems) for read or(and) write.
// Sequential multi-byte read or/and write is possible.
// For some actions, there's no need to read or write, just select address.
//
// EZ-USB uses 11 lines to FPGA. Typical exchange is as follows:
// 1. set direction towards FPGA (vcr_dir=0). Assert CPU I/O controls accordingly.
// 2. assert address on [7:0] vcr_inout.
// 3. assert vcr_set_addr, then deassert vcr_set_addr. This selects address.
// Optionally write:
// 4. setup data byte on [7:0] vcr_inout.
// 5. assert vcr_set_data, then deassert vcr_set_data. This performs write of the byte from step 4.
// 6. go to step 4 if nesessary.
// Optionally read:
// 7. set direction for input from FPGA (vcr_dir=1). Assert CPU I/O controls accordingly.
// 8. read data byte from [7:0] vcr_inout.
// 9. assert vcr_set_data, then deassert vcr_set_data. This let next data byte appear on [7:0] vcr_inout.
// 10. go to step 7 if necessary.
//
//***********************************************************

module vcr_v2(
	input CS,
	
	inout [7:0] vcr_inout, //  Vendor Command/Request (VCR) address/data
	input vcr_clk_in,
	
	input IFCLK,
	
	input [2:0] FPGA_ID,
	input [7:0] hs_io_timeout,
	input hs_input_prog_full,
	input sfifo_not_empty,
	input io_fsm_error, io_err_write,
	input [15:0] output_limit,
	input output_limit_not_done,
	input [7:0] app_status,
	input [7:0] pkt_comm_status, debug2, debug3,
	
	//
	// Defaults for various controls
	//
	output reg hs_en = 0, // high-speed i/o
	output reg output_mode_limit = 1, // output_limit 
	output reg reg_output_limit = 0,
	output reg [7:0] app_mode = 0 // default mode: 0
	);

	wire ENABLE = CS;
	
	wire vcr_dir; // 0: to fpga; 1: from fpga
	wire [7:0] vcr_out;
	assign vcr_inout = ENABLE && vcr_dir ? vcr_out : 8'bz;
	
	(* IOB="true" *)
	reg [7:0] vcr_in_r = 0;
	(* IOB="true" *)
	reg vcr_clk_r = 0;
	reg vcr_clk_r2 = 0;
	always @(posedge IFCLK) begin
		vcr_in_r <= vcr_inout;
		vcr_clk_r <= vcr_clk_in;
		vcr_clk_r2 <= vcr_clk_r;
	end
	
	pulse1 pulse1_vcr_clk( .CLK(IFCLK),
			.sig(vcr_clk_r2 & ENABLE), .out(vcr_clk_en));
	
	// VCR address definitions
	localparam VCR_SET_HS_IO_ENABLE = 8'h80;
	localparam VCR_SET_HS_IO_DISABLE = 8'h81;
	localparam VCR_SET_APP_MODE = 8'h82;
	localparam VCR_GET_IO_STATUS = 8'h84;
	// registers output limit (in output_limit_fifo words); starts
	// output of that many via high-speed interface
	localparam VCR_REG_OUTPUT_LIMIT = 8'h85;
	localparam VCR_SET_OUTPUT_LIMIT_ENABLE = 8'h86;
	localparam VCR_SET_OUTPUT_LIMIT_DISABLE = 8'h87;
	localparam VCR_ECHO_REQUEST = 8'h88;
	localparam VCR_GET_FPGA_ID = 8'h8A;
	localparam VCR_RESET = 8'h8B;
	localparam VCR_GET_ID_DATA = 8'h90;
	localparam VCR_GET_IO_TIMEOUT = 8'h91;
	//localparam VCR_ = 8'h;


	/////////////////////////////////////////////////////////
	//
	// declarations for Command / Request specific stuff
	//
	/////////////////////////////////////////////////////////
	localparam [15:0] BITSTREAM_TYPE = 1;
	reg [7:0] echo_content [3:0];
	reg RESET_R = 0;
	

	reg [7:0] addr = 0;
	reg [3:0] count = 0;
	
	localparam STATE_WAIT = 0,
				STATE_SET_ADDR = 1,
				STATE_WR = 2,
				STATE_RD = 3;
	
	(* FSM_EXTRACT="true" *)
	reg [1:0] state = STATE_WAIT;


	/////////////////////////////////////////////////////////
	//
	// Input
	//
	/////////////////////////////////////////////////////////

	always @(posedge IFCLK) begin
		if (reg_output_limit)
			reg_output_limit <= 0;

		case (state)
		STATE_WAIT: if (vcr_clk_en) begin
			addr <= vcr_in_r;
			count <= 0;
			state <= STATE_SET_ADDR;
		end
		
		STATE_SET_ADDR: begin
			// Addresses for write
			if (addr == VCR_ECHO_REQUEST
					|| addr == VCR_SET_APP_MODE)
				state <= STATE_WR;
			
			// Addresses for read
			else if (addr == VCR_GET_IO_STATUS
					|| addr == VCR_GET_ID_DATA
					|| addr == VCR_GET_FPGA_ID
					|| addr == VCR_GET_IO_TIMEOUT)
				state <= STATE_RD;
				
			// For addresses below, no need to read or write, just select address.
			else if (addr == VCR_SET_HS_IO_ENABLE) begin
				hs_en <= 1;
				state <= STATE_WAIT;	
			end
			else if (addr == VCR_SET_HS_IO_DISABLE) begin
				hs_en <= 0;
				state <= STATE_WAIT;	
			end
			else if (addr == VCR_SET_OUTPUT_LIMIT_ENABLE) begin
				output_mode_limit <= 1;
				state <= STATE_WAIT;	
			end
			else if (addr == VCR_SET_OUTPUT_LIMIT_DISABLE) begin
				output_mode_limit <= 0;
				state <= STATE_WAIT;	
			end
			else if (addr == VCR_REG_OUTPUT_LIMIT) begin
				reg_output_limit <= 1;
				state <= STATE_RD;
			end
			else if (addr == VCR_RESET)
				RESET_R <= 1;

			else // invalid addr
				state <= STATE_WAIT;	
		end
		
		STATE_WR: if (vcr_clk_en) begin
			if (addr == VCR_ECHO_REQUEST) begin
				echo_content[ count[1:0] ] <= vcr_in_r;
				if (count == 3) begin
					state <= STATE_RD;
					count <= 0;
				end
				else
					count <= count + 1'b1;
			end
			else if (addr == VCR_SET_APP_MODE) begin
				app_mode <= vcr_in_r;
				state <= STATE_WAIT;
			end	
		end
		
		STATE_RD: if (vcr_clk_en) begin
			count <= count + 1'b1;
			if (addr == VCR_GET_FPGA_ID
					|| addr == VCR_GET_IO_TIMEOUT
					|| count == 1 && addr == VCR_REG_OUTPUT_LIMIT
					|| count == 1 && addr == VCR_GET_ID_DATA
					|| count == 3 && addr == VCR_ECHO_REQUEST
					|| count == 5 && addr == VCR_GET_IO_STATUS)
				state <= STATE_WAIT;
		end
		endcase
	end

	
	/////////////////////////////////////////////////////////
	//
	// Output
	//
	/////////////////////////////////////////////////////////

	assign vcr_out =
		(addr == VCR_REG_OUTPUT_LIMIT && count == 0) ? output_limit[7:0] :
		(addr == VCR_REG_OUTPUT_LIMIT && count == 1) ? output_limit[15:8] :
		
		(addr == VCR_GET_IO_STATUS && count == 0) ? {
			{2{1'b0}}, io_err_write, io_fsm_error,
			sfifo_not_empty, 1'b0, output_limit_not_done, hs_input_prog_full
		} :
		(addr == VCR_GET_IO_STATUS && count == 1) ? hs_io_timeout :
		(addr == VCR_GET_IO_STATUS && count == 2) ? app_status :
		(addr == VCR_GET_IO_STATUS && count == 3) ? pkt_comm_status :
		(addr == VCR_GET_IO_STATUS && count == 4) ? debug2 :
		(addr == VCR_GET_IO_STATUS && count == 5) ? debug3 :
		
		(addr == VCR_ECHO_REQUEST) ? echo_content[ count[1:0] ] ^ 8'h5A :
		
		(addr == VCR_GET_ID_DATA && count == 0) ? BITSTREAM_TYPE[7:0] :
		(addr == VCR_GET_ID_DATA && count == 1) ? BITSTREAM_TYPE[15:8] :
		
		(addr == VCR_GET_FPGA_ID) ? { {5{1'b0}}, FPGA_ID } :

		(addr == VCR_GET_IO_TIMEOUT) ? hs_io_timeout :
		8'b0;

	assign vcr_dir = state == STATE_RD;

	startup_spartan6 startup_spartan6(.rst(RESET_R));

endmodule

