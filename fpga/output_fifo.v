`timescale 1ns / 1ps

//**********************************************************
//
// Output FIFO (high-speed output).
//
//**********************************************************

module output_fifo (
	input wr_clk,
	input [15:0] din,
	input wr_en,
	output full,

	input rd_clk,
	output [15:0] dout,
	input rd_en,
	output empty,
	input mode_limit,
	input reg_output_limit,
	output [15:0] output_limit,
	output output_limit_not_done
	);

	//
	// output_limit_fifo (one is able to limit output) is unable for asynchronous operation.
	// Because of that, a small asynchronous FIFO is prepend.
	//
	
	// IP Coregen -> FIFO Generator
	// Independent Clocks - Distributed RAM
	// First Word Fall Through
	// width 16, depth 32
	// Reset: off
	//
	wire [15:0] data_stage2;
	fifo_dram_async_16 fifo_dram_async_16(
		.wr_clk(wr_clk),
		.din(din),
		.wr_en(wr_en),
		.full(full),

		.rd_clk(rd_clk),
		.dout(data_stage2),
		.rd_en(rd_en_stage2),
		.empty(empty_stage2)
	);
	assign rd_en_stage2 = ~empty_stage2 & ~full_stage2;
	assign wr_en_stage2 = rd_en_stage2;


	output_limit_fifo #(
		//.ADDR_MSB(12)
		.ADDR_MSB(13)	// 32 Kbytes
	) output_limit_fifo(
		.rst(1'b0),
		.CLK(rd_clk),
		
		.din(data_stage2),
		.wr_en(wr_en_stage2),
		.full(full_stage2),

		.dout(dout),
		.rd_en(rd_en),
		.empty(empty),
		
		.mode_limit(mode_limit),
		.reg_output_limit(reg_output_limit),
		.output_limit(output_limit),
		.output_limit_not_done(output_limit_not_done)
	);

endmodule
