#ifndef NET_LINEAR_LIF_H
#define NET_LINEAR_LIF_H

#include <ap_fixed.h>
#include <ap_int.h>

// Dimensions
#define N_IN      161   // Matches Gamma-Neutron traces
#define N_OUT     1     // One value

typedef ap_fixed<24,8, AP_RND, AP_SAT> data_t; 
typedef ap_uint<1> spike_t;

enum ResetMode { RESET_SUBTRACT=0, RESET_ZERO=1 };

// Core LIF function
void net_linear_lif(
    const data_t x[N_IN],
    const data_t W[N_OUT][N_IN],
    const data_t b[N_OUT],
    data_t mem[N_OUT],
    spike_t spk[N_OUT],
    data_t beta,
    data_t thr,
    ResetMode reset_mode,
    bool spike_on_post_update
);

#endif // NET_LINEAR_LIF_H
