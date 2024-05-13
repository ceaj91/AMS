`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 01/17/2023 05:53:45 PM
// Design Name: 
// Module Name: vga_tb
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module vga_tb(

    );
    
    //SIGNALS
    //globals
	logic         clk = 0;
	 logic         rstn;
	//vga interface
	 logic        vga_hsync;
	 logic        vga_vsync;
	 logic [15:0] vga_rgb;
	//bram interface
	 logic [31:0] bram_addr;
	 logic [31:0] bram_din;
	 logic        bram_en; 
    
	 logic [31:0] addra;
	 logic [31:0] dina;
	 logic [3:0] wea;
    logic [15:0] upper, lower, number;
     
     
     
     integer i = 0;

localparam integer CLK_PERIOD = 100;

  //CLOCK DIRVER
  always 
  begin
     clk = #(CLK_PERIOD/2) !clk; 
  end
  
  // INITIAL
  initial
  begin
    rstn <= 0;
    number <= 0;
    @(posedge clk);
    @(posedge clk);
    rstn <= 1'b1;
    for(i = 0 ; i < 19200; i=i+1) begin
      
     
       lower = number;        
       upper = number+1;    
              dina  = {upper, lower};
  
       addra <= i*4;
     
     
      wea   <= 4'b1111;
      @(posedge clk);
       number = number + 2;
    end
  end
   
   
VGA #() moj_vga
(
 .clk       (clk      ),
 .reset     (rstn    ),
 .vga_hsync (vga_hsync),
 .vga_vsync (vga_vsync),
 .vga_rgb   (vga_rgb  ),
 .bram_addr (bram_addr),
 .bram_din  (bram_din ),
 .bram_en   (bram_en  )
);
   
sdp_bwe_bram #(
		      .NB_COL    (4),
		      .COL_WIDTH (8),
		      .RAM_DEPTH (19200),
		      .RAM_PERFORMANCE ("LOW_LATENCY"),
		      .IN_REG_NUM (0),
		      .INIT_FILE ("")
		      ) moj_bram(
            .clka   (clk  ),
            .clkb   (clk  ),
            .addra  (addra ),
            .dina   (dina  ),
            .wea    (wea   ),
            .addrb  (bram_addr ),
            .doutb  (bram_din ),
            .enb    (bram_en   ),
            .rstb   (!rstn  ),
            .regceb (1'b1)
			 );
   
   

endmodule
