
#include "myproject_auto_accel.h"
#include "myproject.h"

void myproject_auto_accel(
    hls::stream<axis_t> &s_axis_in,
    hls::stream<axis_t> &s_axis_out
) {
#pragma HLS INTERFACE axis      port=s_axis_in
#pragma HLS INTERFACE axis      port=s_axis_out
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL

    input_t in_arr[161];
#pragma HLS ARRAY_PARTITION variable=in_arr complete dim=1

READ_IN:
    for (int i = 0; i < 161; i++) {
    #pragma HLS PIPELINE II=1
        axis_t din = s_axis_in.read();
        in_arr[i] = axis_to_fixed<16,6>(din);
    }

    result_t out_arr[2];
#pragma HLS ARRAY_PARTITION variable=out_arr complete dim=1

    myproject(in_arr, out_arr);

WRITE_OUT:
    for (int i = 0; i < 2; i++) {
    #pragma HLS PIPELINE II=1
        axis_t dout;
        fixed_to_axis<16,6>(dout, out_arr[i]);
        dout.keep = -1; dout.strb = -1;
        dout.user = 0; dout.id = 0; dout.dest = 0;
        dout.last = (i == 2 - 1);
        s_axis_out.write(dout);
    }
}
