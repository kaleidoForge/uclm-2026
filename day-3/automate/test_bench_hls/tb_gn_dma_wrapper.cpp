// =============================================================================
//  tb_gn_dma_wrapper.cpp
//  HLS C++ Testbench — Gamma-Neutron classifier (MLP, 2 outputs)
//  DMA wrapper: AXI-Stream IN → AXI-Stream OUT
//
//  Architecture:
//    - MLP 1D, input_size = 161 features
//    - 2 output classes: [0]=gamma, [1]=neutron
//    - Normalization: per-sample  →  x_norm = 2*(x - min)/(max - min) - 1
//                     applied in software before feeding the DMA wrapper.
//
//  Trace data:
//    - gn_traces_gamma.h   : 50 raw gamma samples   (each: 161 integer values)
//    - gn_traces_neutron.h : 50 raw neutron samples  (each: 161 integer values)
//
//  Test flow per sample:
//    1. Parse raw integers into float array
//    2. Apply per-sample normalization  →  ap_fixed input values
//    3. Push N_INPUT values onto s_axis_in  (one AXIS beat per input element)
//    4. Call dma_wrapper(s_axis_in, s_axis_out)
//    5. Pop N_OUTPUT values from s_axis_out
//    6. argmax(out[0], out[1])  →  predicted class
//    7. Compare with ground truth, accumulate accuracy
//
//  Pass/fail criterion: overall accuracy > PASS_THRESHOLD (default 80 %)
// =============================================================================

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <cassert>

// ---- HLS headers (provided by Vitis HLS) -----------------------------------
#include "ap_int.h"
#include "ap_fixed.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"

// ---- Generated firmware headers --------------------------------------------
//  Adjust these paths to your actual hls4ml project layout:
//    <project>/firmware/myproject.h      → top-level inference function
//    <project>/firmware/dma_wrapper.h    → DMA AXI-Stream wrapper
#include "firmware/myproject.h "
#include "firmware/myproject_auto_accel.h "   // exposes:  void dma_wrapper(hls::stream<axis_t>&, hls::stream<axis_t>&)

// ---- Trace data (raw integers, one sample per line, comma-separated) -------
//  Format expected from hls_wrapper.py generator:
//    static const int gn_gamma[N_GAMMA][N_INPUT]   = { {v0,v1,...}, ... };
//    static const int gn_neutron[N_NEUTRON][N_INPUT] = { ... };
// ----------------------------------------------------------------------------
// NOTE: The .h files ship as flat comma-separated lines (no C-array brackets).
//       We provide a thin wrapper below that repackages them into a 2-D array.

// ---------------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------------
#define N_INPUT          161          // number of features per sample (161 values)
#define N_OUTPUT         2            // number of output classes
#define N_GAMMA_SAMPLES  50           // samples in gn_traces_gamma.h
#define N_NEUTRON_SAMPLES 50          // samples in gn_traces_neutron.h
#define AXIS_W           32           // AXIS bus width (bits) -- must match wrapper

// Pass/fail threshold (fraction: 0.0 – 1.0)
#define PASS_THRESHOLD   0.80f

// Class labels (must match softmax output order from hls4ml)
#define CLASS_GAMMA      0
#define CLASS_NEUTRON    1

// ---------------------------------------------------------------------------
//  Raw trace data: repackaged from the flat .h files
//  Each .h file contains N_SAMPLES lines; each line is 161 comma-separated ints.
//  We convert them into plain C 2-D int arrays here.
// ---------------------------------------------------------------------------

// ---- Gamma traces ----------------------------------------------------------
// gn_traces_gamma.h: 50 samples × 161 values
static const int raw_gamma[N_GAMMA_SAMPLES][N_INPUT] = {
#include "gn_traces_gamma.h"
};

// ---- Neutron traces --------------------------------------------------------
// gn_traces_neutron.h: 50 samples × 161 values
static const int raw_neutron[N_NEUTRON_SAMPLES][N_INPUT] = {
#include "gn_traces_neutron.h"
};

// ---------------------------------------------------------------------------
//  Per-sample normalization
//  Mirrors the Python implementation in KalEdge:
//    x_norm = 2 * (x - x_min) / (x_max - x_min) - 1
//  Output is clamped to [-1, 1] to match the ap_fixed range.
// ---------------------------------------------------------------------------
static void normalize_per_sample(
    const int   raw[N_INPUT],
    float       norm[N_INPUT]
) {
    // Find min and max
    int vmin = raw[0], vmax = raw[0];
    for (int i = 1; i < N_INPUT; i++) {
        if (raw[i] < vmin) vmin = raw[i];
        if (raw[i] > vmax) vmax = raw[i];
    }
    float range = (vmax - vmin == 0) ? 1.0f : static_cast<float>(vmax - vmin);

    for (int i = 0; i < N_INPUT; i++) {
        float v = 2.0f * (static_cast<float>(raw[i]) - static_cast<float>(vmin)) / range - 1.0f;
        // clamp to [-1, 1] for safety
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        norm[i] = v;
    }
}

// ---------------------------------------------------------------------------
//  AXIS packing helpers (must match dma_wrapper.h / hls_wrapper.py)
//  The wrapper packs each input element into the LSB of a 32-bit AXIS word.
// ---------------------------------------------------------------------------

// Encode a normalized float into an ap_fixed<W,I> and then into an axis_t word.
// W and I must match the input_t typedef in defines.h 
// *** Adjust INPUT_W / INPUT_I if your defines.h differs ***
#ifndef INPUT_W
#define INPUT_W 16
#endif
#ifndef INPUT_I
#define INPUT_I 6
#endif

// Same for output (result_t)
#ifndef OUTPUT_W
#define OUTPUT_W 16
#endif
#ifndef OUTPUT_I
#define OUTPUT_I 6
#endif

typedef ap_axiu<AXIS_W, 1, 1, 1> axis_t;

static axis_t float_to_axis_input(float val) {
    ap_fixed<INPUT_W, INPUT_I> fxd = val;   // float: ap_fixed (saturating)
    axis_t a;
    a.data = 0;
    a.data.range(INPUT_W - 1, 0) = fxd.range(INPUT_W - 1, 0);
    a.keep = -1;
    a.strb = -1;
    a.user = 0;
    a.id   = 0;
    a.dest = 0;
    a.last = 0;                              // set last on the final beat below
    return a;
}

static float axis_to_float_output(const axis_t &a) {
    ap_fixed<OUTPUT_W, OUTPUT_I> fxd;
    fxd.range(OUTPUT_W - 1, 0) = a.data.range(OUTPUT_W - 1, 0);
    return static_cast<float>(fxd);
}

// ---------------------------------------------------------------------------
//  Run one sample through the DMA wrapper
//  Returns the predicted class index (argmax of the two outputs).
// ---------------------------------------------------------------------------
static int run_sample(
    const int raw[N_INPUT],
    float     out_scores[N_OUTPUT]   // output: raw softmax scores (optional)
) {
    // 1. Per-sample normalization
    float norm[N_INPUT];
    normalize_per_sample(raw, norm);

    // 2. Build input AXI-Stream
    hls::stream<axis_t> s_in("s_in");
    for (int i = 0; i < N_INPUT; i++) {
        axis_t beat = float_to_axis_input(norm[i]);
        beat.last = (i == N_INPUT - 1) ? 1 : 0;
        s_in.write(beat);
    }

    // 3. Run DMA wrapper (calls myproject internally)
    hls::stream<axis_t> s_out("s_out");
    myproject_auto_accel(s_in, s_out);

    // 4. Read output AXI-Stream
    float scores[N_OUTPUT];
    for (int i = 0; i < N_OUTPUT; i++) {
        axis_t beat = s_out.read();
        scores[i] = axis_to_float_output(beat);
    }

    // 5. Verify streams are empty
    if (!s_in.empty()) {
        std::cerr << "[WARN] s_in not fully consumed after wrapper call!\n";
    }

    // 6. Argmax
    int pred = 0;
    for (int c = 1; c < N_OUTPUT; c++) {
        if (scores[c] > scores[pred]) pred = c;
    }

    // Optional: expose scores to caller
    if (out_scores) {
        for (int c = 0; c < N_OUTPUT; c++) out_scores[c] = scores[c];
    }

    return pred;
}

// ---------------------------------------------------------------------------
//  Main testbench
// ---------------------------------------------------------------------------
int main() {
    std::cout << "============================================================\n";
    std::cout << "  HLS Testbench: Gamma-Neutron DMA Wrapper\n";
    std::cout << "  Architecture : MLP 1D, input=" << N_INPUT
              << " features, output=" << N_OUTPUT << " classes\n";
    std::cout << "  Normalization: per-sample  →  [-1, 1]\n";
    std::cout << "  Input dtype  : ap_fixed<" << INPUT_W << "," << INPUT_I << ">\n";
    std::cout << "  Output dtype : ap_fixed<" << OUTPUT_W << "," << OUTPUT_I << ">\n";
    std::cout << "============================================================\n\n";

    int total   = 0;
    int correct = 0;

    float out_scores[N_OUTPUT];

    // ---- Test Gamma samples (ground truth = CLASS_GAMMA = 0) ---------------
    std::cout << "--- GAMMA samples (expected class " << CLASS_GAMMA << ") ---\n";
    std::cout << std::setw(6)  << "idx"
              << std::setw(10) << "score[0]"
              << std::setw(10) << "score[1]"
              << std::setw(8)  << "pred"
              << std::setw(8)  << "gt"
              << std::setw(8)  << "OK?"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (int s = 0; s < N_GAMMA_SAMPLES; s++) {
        int pred = run_sample(raw_gamma[s], out_scores);
        int gt   = CLASS_GAMMA;
        bool ok  = (pred == gt);
        total++;
        if (ok) correct++;

        std::cout << std::setw(6)  << s
                  << std::fixed << std::setprecision(4)
                  << std::setw(10) << out_scores[0]
                  << std::setw(10) << out_scores[1]
                  << std::setw(8)  << pred
                  << std::setw(8)  << gt
                  << std::setw(8)  << (ok ? "PASS" : "FAIL")
                  << "\n";
    }

    // ---- Test Neutron samples (ground truth = CLASS_NEUTRON = 1) -----------
    std::cout << "\n--- NEUTRON samples (expected class " << CLASS_NEUTRON << ") ---\n";
    std::cout << std::setw(6)  << "idx"
              << std::setw(10) << "score[0]"
              << std::setw(10) << "score[1]"
              << std::setw(8)  << "pred"
              << std::setw(8)  << "gt"
              << std::setw(8)  << "OK?"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (int s = 0; s < N_NEUTRON_SAMPLES; s++) {
        int pred = run_sample(raw_neutron[s], out_scores);
        int gt   = CLASS_NEUTRON;
        bool ok  = (pred == gt);
        total++;
        if (ok) correct++;

        std::cout << std::setw(6)  << s
                  << std::fixed << std::setprecision(4)
                  << std::setw(10) << out_scores[0]
                  << std::setw(10) << out_scores[1]
                  << std::setw(8)  << pred
                  << std::setw(8)  << gt
                  << std::setw(8)  << (ok ? "PASS" : "FAIL")
                  << "\n";
    }

    // ---- Summary ------------------------------------------------------------
    float accuracy = static_cast<float>(correct) / static_cast<float>(total);

    std::cout << "\n============================================================\n";
    std::cout << "  RESULTS SUMMARY\n";
    std::cout << "============================================================\n";
    std::cout << "  Total samples  : " << total   << "\n";
    std::cout << "  Correct        : " << correct  << "\n";
    std::cout << "  Wrong          : " << (total - correct) << "\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Accuracy       : " << (accuracy * 100.0f) << " %\n";
    std::cout << "  Pass threshold : " << (PASS_THRESHOLD * 100.0f) << " %\n";
    std::cout << "------------------------------------------------------------\n";

    if (accuracy >= PASS_THRESHOLD) {
        std::cout << "  RESULT: *** TESTBENCH PASSED ***\n";
        std::cout << "============================================================\n";
        return 0;    // Vitis HLS: 0 = PASS
    } else {
        std::cout << "  RESULT: *** TESTBENCH FAILED (accuracy too low) ***\n";
        std::cout << "============================================================\n";
        return 1;    // Vitis HLS: non-zero = FAIL
    }
}
