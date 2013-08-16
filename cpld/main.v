`timescale 1ns / 1ps

module main(
    input cart_fi2,
    output cart_fi2_copy,
    input fi2,
    input cart_s4,
    input cart_s5,
    input cart_rw,
    /*input cart_cctl,*/
    input [12:0] cart_addr,
    inout [7:0] cart_data,
    output ram_oe,
    output ram_we,
    output [14:0] ram_addr,
    inout [7:0] ram_data,
    input clk,
    inout [7:0] uc_data,
    output reg uc_ack = 0,
    input uc_read,
    input uc_write,
    input set_addr_lo,
    input set_addr_hi,
    input strobe_addr,
    output aux0,
    output aux1,
    /*input aux2,*/
    /*input aux3,*/
    /*input aux4,*/
    /*input aux5,*/
    output dbg0,
    output dbg1);

wire cart_select;
wire fi2_falling;
wire fi2_rising;

reg state_cart_write = 0;
reg state_cart_read = 0;
reg state_uc_write = 0;
reg state_uc_read = 0;
reg [1:0] phase = 2'b01;

reg [1:0] fi2_r = 2'b00;

reg [7:0] cart_out_data_latch;

reg [13:0] uc_addr = 0;
reg [7:0] uc_out_data_latch = 0;

reg cart_write_enable = 0;

assign cart_fi2_copy = cart_fi2;

assign fi2_falling = fi2_r[1] & ~fi2_r[0];
assign fi2_rising = ~fi2_r[1] & fi2_r[0];

assign cart_select = cart_s4 ^ cart_s5;

assign cart_data = (cart_select & cart_rw) ? cart_out_data_latch : 8'hzz;

assign ram_addr = (state_cart_write | state_cart_read) ? {1'b0, cart_s4, cart_addr} :
                  {1'b0, uc_addr};
assign ram_data = state_cart_write ? cart_data :
                  state_uc_write ? uc_data :
                  8'hzz;

assign uc_data = uc_read ? uc_out_data_latch : 8'hzz;

always @(posedge strobe_addr)
    if (set_addr_lo)
        uc_addr[7:0] <= uc_data;
    else if (set_addr_hi)
        uc_addr[13:8] <= uc_data[5:0];
    else
        uc_addr <= uc_addr + 1;

always @(posedge clk) begin
    fi2_r <= {fi2_r[0], fi2};
    
    if (state_cart_write | state_cart_read | state_uc_write | state_uc_read)
        case (phase)
            2'b01: phase <= 2'b11;
            2'b11: phase <= 2'b10;
            2'b10: phase <= 2'b00;
            2'b00: phase <= 2'b01;
        endcase

    case ({state_cart_write, state_cart_read, state_uc_write, state_uc_read})
        // idle
        4'b0000:
            if (fi2_rising & ~cart_rw & cart_select)
                state_cart_write <= 1;
            else if (fi2_rising & cart_rw & cart_select)
                state_cart_read <= 1;
            else if (fi2_falling & uc_write & ~uc_ack)
                state_uc_write <= 1;
            else if (fi2_falling & uc_read & ~uc_ack)
                state_uc_read <= 1;

        // cart write
        4'b1000:
            if (phase == 2'b00)
                state_cart_write <= 0;

        // cart read
        4'b0100:
            if (phase == 2'b00)
                state_cart_read <= 0;

        // uc write
        4'b0010:
            if (phase == 2'b00)
                state_uc_write <= 0;

        // uc read
        4'b0001:
            if (phase == 2'b00)
                state_uc_read <= 0;
    endcase

    if (state_cart_write & phase == 2'b00)
        cart_write_enable <= 1;

    if (state_cart_read & phase == 2'b10)
        cart_out_data_latch <= ram_data;

    if (state_uc_read & phase == 2'b10)
        uc_out_data_latch <= ram_data;

    if ((state_uc_write | state_uc_read) & phase == 2'b00)
        uc_ack <= 1;
    else if (~uc_write & ~uc_read)
        uc_ack <= 0;
end

assign ram_oe = ~(state_cart_read | state_uc_read);
assign ram_we = ~(((state_cart_write & cart_write_enable) | state_uc_write) & |phase);

assign dbg0 = state_cart_write;
assign dbg1 = state_cart_read;
assign aux0 = state_uc_write;
assign aux1 = state_uc_read;

endmodule
