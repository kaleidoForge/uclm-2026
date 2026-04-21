// KaleidoForge

#include "net_linear_lif.h"

void net_linear_lif(
    const data_t x[N_IN],
    const data_t W[N_OUT][N_IN],
    const data_t b[N_OUT],
    data_t mem[N_OUT],          // estado por neurona (persistente)
    spike_t spk[N_OUT],         // spikes de salida
    data_t beta,
    data_t thr,
    ResetMode reset_mode,
    bool spike_on_post_update   // true: compara U_lin; false: compara U
) {
#pragma HLS PIPELINE off


OUT:
    for (int o = 0; o < N_OUT; o++) {

        // 1) Linear: I = W x + b
        data_t acc = b[o];
    IN:
        for (int i = 0; i < N_IN; i++) {
            acc += W[o][i] * x[i];
        }

        // 2) Leak + integrate
        data_t U = mem[o];
        data_t U_lin = beta * U + acc;

        // 3) Spike decision
        spike_t S = (spike_on_post_update) ? (U_lin > thr) : (U > thr);
        spk[o] = S;

        // 4) Reset
        data_t U_next;
        if (reset_mode == RESET_SUBTRACT) {
            U_next = U_lin - (data_t)S * thr;
        } else { // RESET_ZERO
            U_next = U_lin - (data_t)S * U_lin;
        }

        mem[o] = U_next;
    }
}
