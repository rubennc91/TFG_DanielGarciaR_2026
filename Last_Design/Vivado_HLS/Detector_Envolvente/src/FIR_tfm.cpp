/**************************************************************
· file   FIR_tfm.cpp.h
· author Daniel García
· date   2026-04-10
· description:

*************************************************************/
#include "config.h"
#include "FIR_filter.h"
#include "hls_math.h"
#include <ap_axi_sdata.h>
#include <ap_int.h>

void FIR_filter_tfm(
        hls::stream<TL_data_raw>& data_in_stream,
        hls::stream<axis_out_t>& data_out_stream
) {
#pragma HLS INTERFACE axis register both port=data_in_stream
#pragma HLS INTERFACE axis register both port=data_out_stream
#pragma HLS INTERFACE ap_ctrl_hs port=return

    // Leer muestra de entrada
    TL_data_raw aux_input_raw = data_in_stream.read();

    int sample_idx = 0; // 0 = STATUS, 1-8 = canales
    int channel = 0;    // 0-7 para tus buffers internos

    sample_idx = aux_input_raw.range(31, 28);

    // Estado del filtro
    static Q_suma media_movil[num_canales] = {0};
#pragma HLS ARRAY_PARTITION variable=media_movil complete dim=1

    // IGNORAR CANAL 0 (STATUS)
    if (sample_idx == 0) {
       // sample_idx++;
    	channel = 0;
        return;
    }
    channel = sample_idx - 1;
    TL_data adc_value;
    adc_value.range(23, 0) = aux_input_raw.range(23, 0);

    // FIR
    static TL_data fir_shift_reg[num_canales][num_coefs] = {0};
#pragma HLS ARRAY_PARTITION variable=fir_shift_reg complete dim=1

    TL_acum acum_fir_local = 0;

    loop_fir: for (int i = num_coefs - 1; i >= 0; i--) {
#pragma HLS PIPELINE II=1
        TL_data val = (i == 0) ? adc_value : fir_shift_reg[channel][i - 1];
        fir_shift_reg[channel][i] = val;
        acum_fir_local += val * FIR_coefs[i];
    }

    // DETECTOR DE ENVOLVENTE
    TL_acum aux_input_det = (acum_fir_local < 0) ?
                           (TL_acum)-acum_fir_local :
                           (TL_acum)acum_fir_local;

    static TL_acum shift_reg[num_canales][num_coefs_det] = {0};
#pragma HLS ARRAY_PARTITION variable=shift_reg complete dim=1
#pragma HLS ARRAY_PARTITION variable=shift_reg cyclic factor=4 dim=2

    loop_shift_det: for (int i = num_coefs_det - 1; i > 0; i--) {
#pragma HLS PIPELINE II=1
        shift_reg[channel][i] = shift_reg[channel][i - 1];
    }
    shift_reg[channel][0] = aux_input_det;

    // SUMA
    const int UR = 4;
    Q_suma acum[UR];
#pragma HLS ARRAY_PARTITION variable=acum complete dim=1

    loop_unroll_acum: for (int i = 0; i < UR; i++) {
#pragma HLS UNROLL
        acum[i] = 0;
        for (int k = 0; k < num_coefs_det / UR; k++) {
#pragma HLS PIPELINE
            int idx = k + i * (num_coefs_det / UR);
            acum[i] += shift_reg[channel][idx];
        }
    }

    Q_suma total_suma = 0;
    for (int i = 0; i < UR; i++) {
#pragma HLS UNROLL
        total_suma += acum[i];
    }

    media_movil[channel] = total_suma >> 8;

    // SALIDA AXI STREAM
    axis_out_t paquete;
    paquete.data = media_movil[channel];
    paquete.keep = -1;

    // TLAST cada 8 canales válidos
    if (channel == (num_canales - 1)) {
        paquete.last = 1;
       // channel = 0;
    } else {
        paquete.last = 0;
       // channel++;
    }

    data_out_stream.write(paquete);

    // ACTUALIZAR ÍNDICE ADS
    //sample_idx++;

   // if (sample_idx == (num_canales + 1)) { // 0..8
    //    sample_idx = 0;
   // }
}
