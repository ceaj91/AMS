`timescale 1ns / 1ps

module VGA# ()
   (
    //globals
    input logic         clk,
    input logic         reset,
    //vga interface
    output logic        vga_hsync,
    output logic        vga_vsync,
    output logic [15:0] vga_rgb,
    //bram interface
    output logic [31:0] bram_addr,
    input  logic [31:0] bram_din,
    output logic        bram_en
    );

   logic                         clk_en=0;
   //constants
   localparam integer CLK_DIV_VAL = 4;
   localparam logic [9:0]        h_end = 800;
   localparam logic [9:0]        h_left_boundary = 48;
   localparam logic [9:0]        h_right_boundary = 16;
   localparam logic [9:0]        h_writeback = 96;
   localparam logic [9:0]        h_image = 640;

   localparam logic [9:0]        v_end = 525;
   localparam logic [9:0]        v_bottom_boundary = 10;
   localparam logic [9:0]        v_upper_boundary = 33;
   localparam logic [9:0]        v_writeback = 2;
   localparam logic [9:0]        v_image = 480;
   
   localparam logic [9:0]        h_resolution = 640;
   localparam logic [9:0]        v_resolution = 480;
   
   // registers
   logic [9:0]                   h_counter_reg, h_counter_next;
   logic [9:0]                   v_counter_reg, v_counter_next;

   logic                         hsync_reg, hsync_next;
   logic                         vsync_reg, vsync_next;
   logic [15:0]                  vga_rgb_16;
   
   logic                         himage_active_reg, himage_active_next;
   logic                         vimage_active_reg, vimage_active_next;
   
   logic [$clog2(CLK_DIV_VAL):0] clock_divider_reg, clock_divider_next;
   
   
   logic [31:0] bram_addr_s ;

   logic [31:0] bram_addr_d ;
   
   //sequential logic
   always_ff @(posedge clk)
   begin
      if (!reset) 
      begin
	 clock_divider_reg <= 0;
	 h_counter_reg <= 0;
	 v_counter_reg <= 0;         
	 hsync_reg <= 0;
	 vsync_reg <= 0;
	 himage_active_reg <= 0;
	 vimage_active_reg <= 0;
      end
      else 
      begin
	 clock_divider_reg <= clock_divider_next;
	 h_counter_reg <= h_counter_next;
	 v_counter_reg <= v_counter_next;
	 hsync_reg <= hsync_next;
	 vsync_reg <= vsync_next;
	 himage_active_reg <= himage_active_next;
	 vimage_active_reg <= vimage_active_next;
      end
   end

   //clock divider counter *** generate next state and clock enable
   always_comb
   begin
      clk_en = 1'b0;
      clock_divider_next = clock_divider_reg;      
      if(clock_divider_reg == (CLK_DIV_VAL - 1))
      begin
	 clock_divider_next = 0;
	 clk_en = 1'b1;
      end
      else 
      begin
	 clock_divider_next = clock_divider_reg + 1;                 
	 clk_en = 1'b0;
      end            
   end

   //horizontal counter *** generate next state 
   always_comb 
   begin
      h_counter_next = h_counter_reg;  
      if (clk_en == 1) 
      begin
	 if (h_counter_reg == (h_end - 1))
	 begin
	    h_counter_next = 0;
	 end
	 else
	 begin
	    h_counter_next = h_counter_reg + 1;
	 end
      end
   end

   //vertical counter *** generate next state 
   always_comb
   begin
      v_counter_next = v_counter_reg;      
      if ((clk_en == 1) && (h_counter_reg == (h_end - 1)))
      begin
	 if (v_counter_reg == (v_end - 1))
	 begin
	    v_counter_next = 0;            
	 end
	 else
	 begin
	    v_counter_next = v_counter_reg + 1;        
	 end
      end
   end

   //horizontal sync signal register *** generate next state 
   always_comb
   begin
      hsync_next = hsync_reg;
      himage_active_next = himage_active_reg;    
      
      if(h_counter_reg >= 0 && h_counter_reg < h_image)
	himage_active_next = 1'b1;
      else
	himage_active_next = 1'b0;
      
      // 655 <= h_counter <= 751 
      if (h_counter_reg >= (h_image + h_right_boundary) && h_counter_reg <= (h_image + h_right_boundary + h_writeback - 1))
      begin
	 hsync_next = 1'b0;
      end
      else
      begin
	 hsync_next = 1'b1;
      end
   end

   //vertical sync signal register *** generate next state 
   always_comb
   begin
      vsync_next = vsync_reg;
      vimage_active_next = vimage_active_reg;
      
      
      if(v_counter_reg >= 0 && v_counter_reg < v_image)
	vimage_active_next = 1'b1;
      else
	vimage_active_next = 1'b0;
      // 490 <= h_counter <= 491
      if ((v_counter_reg >= (v_image + v_bottom_boundary)) && (v_counter_reg <= (v_image + v_bottom_boundary + v_writeback - 1)))
      begin
	 vsync_next = 1'b0;         
      end
      else
      begin
	 vsync_next = 1'b1;
      end
   end

   //bram interface *** generate address space for bram
   // *4 part is here as we use 32-bit adress (byte adressible property of BRAM),
   // division by two is because of limited bram, each pixel will actually be 2x2 pixels in 640x480 resolution
   assign bram_addr_s = ((v_counter_reg[9:1] * h_resolution[9:1]) + h_counter_reg[9:1])*2;    
   assign bram_addr = {bram_addr_s [31:2], 2'b00};
   always_ff @(posedge clk)
   begin
      if (!reset) 
	bram_addr_d <= 0;
      else 
	bram_addr_d <= bram_addr_s;
   end


   
   // Choose upper or lower pixel for rgb  output
   
   always_comb
   begin
      if(bram_addr_d[1]==1'b0)
	vga_rgb_16 = bram_din[15:0];
      else
	vga_rgb_16 = bram_din[31:16];
   end
   
   always_comb
   begin
      if(vimage_active_reg == 1'b1 && himage_active_reg == 1'b1)
	vga_rgb = vga_rgb_16;
      else
	vga_rgb = 16'h0000;
   end

   assign vga_vsync = vsync_reg;
   assign vga_hsync = hsync_reg;
   
   assign bram_en = 1'b1;

endmodule
