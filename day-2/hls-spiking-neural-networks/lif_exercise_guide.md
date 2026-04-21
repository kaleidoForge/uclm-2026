# Exercise Guide: LIF Neuron Implementation in FPGA

This guide explains the inner workings of the **Leaky Integrate-and-Fire (LIF)** neuron model developed in Vitis HLS and proposes a series of practical exercises to optimize the design using directives (pragmas).

## 1. Core Code Explanation

The file `net_linear_lif.cpp` contains the main logic of the neuron. The function performs three fundamental steps for each input sample:

### A. Linear Integration ($I = W \cdot x + b$)
The weighted sum of the current inputs is calculated.
```cpp
data_t acc = b[o];
for (int i = 0; i < N_IN; i++) {
    acc += W[o][i] * x[i];
}
```
*   `N_IN`: Number of time samples (161 for the GN dataset).
*   `data_t`: Fixed-point data type (`ap_fixed<24,8>`) to balance precision and resource savings (DSPs).

### B. Leak + Integrate ($U[t] = \beta \cdot U[t-1] + I$)
Leakage is applied to the previous membrane potential, and the current integrated input is added.
```cpp
data_t U = mem[o];
data_t U_lin = beta * U + acc;
```
*   `beta`: Decay factor (typically between 0 and 1).

### C. Spike & Reset
The function decides if the neuron fires (spikes) and resets the potential.
```cpp
spike_t S = (spike_on_post_update) ? (U_lin > thr) : (U > thr);

// Reset (Subtract or Zero)
if (reset_mode == RESET_SUBTRACT) {
    U_next = U_lin - (data_t)S * thr;
} else {
    U_next = U_lin - (data_t)S * U_lin;
}
```

---

## 2. Practical Exercise: Hardware Optimization

The goal is to observe how HLS directives affects area (resources) and timing (latency/throughput).


### Task 1: Baseline Synthesis
Run synthesis without any active directives.
*   **Record**: Latency (cycles) and Initiation Interval (II).
*   **Resources**: How many DSPs are used? (It should be 1 if the loop is sequential).

### Task 2: Spatial Parallelism (Unroll)
Add `#pragma HLS UNROLL factor=4` inside the `IN` loop.
*   **Question**: What happens to the latency? And to the DSP usage?
*   **Challenge**: Why can't you use a full `UNROLL` (`factor=161`) without changing anything else? (Hint: Look for the bottleneck in memory access for `x`).

### Task 3: Memory Partitioning (Array Partition)
Add `#pragma HLS ARRAY_PARTITION variable=x complete` before the loops.
*   **Observation**: Combine this with a larger `UNROLL` factor. Observe how increasing the read ports allows the FPGA to perform more multiplications in parallel.

### Task 4: Pipelining
Add `#pragma HLS PIPELINE II=1` to the main loop.
*   **Analysis**: Compare the "Interval" in the report. Pipelining allows the hardware to start processing the next set of data before the current one finishes.

---

## 3. Results Comparison Table

Complete the following table with the data obtained from the synthesis reports:

| Configuration | Latency (Cycles) | Interval (II) | DSPs | LUTs | FFs |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Baseline** (No pragmas) | | | | | |
| **Unroll factor=4** | | | | | |
| **Unroll + Partition** | | | | | |
| **Final Optimized** | | | | | |

---

## 4. Reflection Questions

1.  **Trade-offs**: What is the cost in LUTs/DSPs of reducing latency? Is it worth it for this specific application?
2.  **Fixed-Point**: If you change `ap_fixed<24,8>` to `float`, what impact does it have on FPGA utilization (DSPs and FFs)?
3.  **Bottleneck**: In this design, what limits performance the most: the speed of the multiplier or the number of memory ports?

----


KaleidoForge 2026 