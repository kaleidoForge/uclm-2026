#include "xparameters.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xmyproject_auto_accel.h"
#include <math.h>

#define DMA_DEV_ID      XPAR_AXIDMA_0_DEVICE_ID
#define ACCEL_DEV_ID    XPAR_MYPROJECT_AUTO_ACCEL_0_DEVICE_ID

#define SIGNAL_SIZE 161

// Buffers (alineados para DMA)
static int32_t input_buffer[SIGNAL_SIZE]  __attribute__ ((aligned(64)));
static int32_t output_buffer[2] __attribute__((aligned(64)));

XAxiDma AxiDma;
XMyproject_auto_accel Accel;

//--------------------------------------------------
// Inicializar DMA
//--------------------------------------------------
int init_dma() {
    XAxiDma_Config *CfgPtr;

    xil_printf("DMA: LookupConfig...\r\n");
    CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) {
        xil_printf("DMA: LookupConfig FALLO\r\n");
        return XST_FAILURE;
    }

    xil_printf("DMA: CfgInitialize...\r\n");
    if (XAxiDma_CfgInitialize(&AxiDma, CfgPtr) != XST_SUCCESS) {
        xil_printf("DMA: CfgInitialize FALLO\r\n");
        return XST_FAILURE;
    }

    // Reset DMA
    XAxiDma_Reset(&AxiDma);
    int timeout = 10000;
    while (!XAxiDma_ResetIsDone(&AxiDma)) {
        if (--timeout == 0) {
            xil_printf("DMA reset TIMEOUT\r\n");
            return XST_FAILURE;
        }
    }
    xil_printf("DMA: Reset OK\r\n");

    xil_printf("DMA: HasSg check...\r\n");
    if (XAxiDma_HasSg(&AxiDma)) {
        xil_printf("DMA en modo SG no soportado\r\n");
        return XST_FAILURE;
    }

    xil_printf("DMA: OK\r\n");
    return XST_SUCCESS;
}

//--------------------------------------------------
// Inicializar Acelerador
//--------------------------------------------------
int init_accel() {
    XMyproject_auto_accel_Config *cfg;

    xil_printf("ACCEL: LookupConfig...\r\n");
    cfg = XMyproject_auto_accel_LookupConfig(ACCEL_DEV_ID);
    if (!cfg) {
        xil_printf("ACCEL: LookupConfig FALLO\r\n");
        return XST_FAILURE;
    }

    xil_printf("ACCEL: CfgInitialize...\r\n");
    if (XMyproject_auto_accel_CfgInitialize(&Accel, cfg) != XST_SUCCESS) {
        xil_printf("ACCEL: CfgInitialize FALLO\r\n");
        return XST_FAILURE;
    }

    xil_printf("ACCEL: OK\r\n");
    return XST_SUCCESS;
}

//--------------------------------------------------
// Ejecutar inferencia
//--------------------------------------------------

int run_inference() {
    XAxiDma_SimpleTransfer(&AxiDma,
        (UINTPTR)output_buffer, 2*sizeof(int32_t), XAXIDMA_DEVICE_TO_DMA);
    XMyproject_auto_accel_Start(&Accel);
    XAxiDma_SimpleTransfer(&AxiDma,
        (UINTPTR)input_buffer, SIGNAL_SIZE * sizeof(int32_t), XAXIDMA_DMA_TO_DEVICE);

    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE));
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA));
    return 0;
}

int main() {
    xil_printf("Init...\r\n");
    Xil_DCacheDisable();
    if (init_dma() != XST_SUCCESS) {
        xil_printf("Error DMA\r\n");
        return -1;
    }

    if (init_accel() != XST_SUCCESS) {
        xil_printf("Error ACCEL\r\n");
        return -1;
    }

    // Neutron
    int16_t pixel[161] = {
    		385,384,385,386,385,385,385,384,385,385,385,384,384,382,381,380,376,374,370,368,364,360,354,351,346,342,335,329,324,318,312,306,300,294,287,282,275,269,265,259,252,248,241,237,233,229,224,219,214,210,205,200,196,192,188,184,182,178,175,171,168,165,160,157,154,151,147,144,141,137,135,131,129,126,123,119,117,114,110,109,106,104,101,100,97,95,93,91,89,88,86,86,84,82,81,79,79,77,76,75,75,74,74,73,72,70,69,68,67,64,63,62,60,59,57,57,54,53,52,51,50,47,45,44,44,43,41,41,40,38,39,36,37,36,37,35,35,34,33,34,34,33,31,32,30,31,30,31,30,31,31,31,31,30,32,31,31,32,33,33,33,
    };

    

    // Encontrar min y max
    int16_t min_val = pixel[0], max_val = pixel[0];
    for (int i = 1; i < SIGNAL_SIZE; i++) {
        if (pixel[i] < min_val) min_val = pixel[i];
        if (pixel[i] > max_val) max_val = pixel[i];
    }

    // Normalizar a [-1, 1] y convertir a fixed point Q<16,6>
    int16_t range = max_val - min_val;
    for (int i = 0; i < SIGNAL_SIZE; i++) {
        // 2 * (x - min) / (max - min) - 1  →  escala a fixed Q6
        float norm = 2.0f * (pixel[i] - min_val) / (float)range - 1.0f;
	input_buffer[i] = (int32_t)(int16_t)(norm * (1 << 10));
    }


    run_inference();

    int16_t raw0 = (int16_t)(output_buffer[0] & 0xFFFF);  // gamma
    int16_t raw1 = (int16_t)(output_buffer[1] & 0xFFFF);  // neutron

    if (raw0 >= raw1)
        xil_printf("GAMMA\r\n");
    else
        xil_printf("NEUTRON\r\n");

    return 0;
}

