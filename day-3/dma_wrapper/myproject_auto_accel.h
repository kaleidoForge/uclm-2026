
#ifndef MYPROJECT_AUTO_ACCEL_H_
#define MYPROJECT_AUTO_ACCEL_H_

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "myproject.h"


#define AXIS_W 32
typedef ap_axiu<AXIS_W,1,1,1> axis_t;

// ---- AXIS packing helpers (LSB payload) ----
template<int W, int I>
static ap_fixed<W,I> axis_to_fixed(const axis_t &a) {
    ap_fixed<W,I> x;
    x.range(W-1,0) = a.data.range(W-1,0);
    return x;
}

template<int W>
static ap_uint<W> axis_to_uint(const axis_t &a) {
    ap_uint<W> x;
    x.range(W-1,0) = a.data.range(W-1,0);
    return x;
}

template<int W, int I>
static void fixed_to_axis(axis_t &ao, const ap_fixed<W,I> &x) {
    ao.data.range(W-1,0) = x.range(W-1,0);
    ao.data.range(31,W)  = 0;
}

template<int W>
static void uint_to_axis(axis_t &ao, const ap_uint<W> &x) {
    ao.data.range(W-1,0) = x.range(W-1,0);
    ao.data.range(31,W)  = 0;
}


void myproject_auto_accel(
    hls::stream<axis_t> &s_axis_in,
    hls::stream<axis_t> &s_axis_out
);

#endif
