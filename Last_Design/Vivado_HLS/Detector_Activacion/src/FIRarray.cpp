/**************************************************************
· file      FIRarray.cpp
· author    Daniel García
· date      2026-02-07
· description:
    Implementación del filtro FIR multicanal con detector de envolvente
    y umbralización con histéresis para señales ADS1298.
    Se procesan 8 canales por frame, sincronizados con TLAST.
*************************************************************/

#include "config.h"
#include "FIR.h"
#include "hls_math.h"
#include <ap_int.h>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>

void FIR_filter(
    hls::stream<TL_data_raw>&      data_in_stream,
    hls::stream<axis_out_t>&   data_out_stream,
    Q_suma                     umbral[num_canales]
) {
#pragma HLS INTERFACE axis register both port=data_in_stream
#pragma HLS INTERFACE axis register both port=data_out_stream
#pragma HLS INTERFACE s_axilite port=umbral bundle=umbral

    enum t_estado { ST_IDLE, ST_ACTIVE };



    // Lectura del stream (24 bits crudos)
    TL_data_raw lectura = data_in_stream.read();

    // Contador de palabras dentro de la trama (0 = STATUS, 1..8 = canales)
    int pkt_count = lectura.range(31, 28);
    TL_data aux_input_raw;
    aux_input_raw.range(23, 0) = lectura.range(23, 0);

    // Variables estáticas para los filtros y estados
    static Q_suma     media_movil[num_canales] = {0};
    static t_estado   estado_actual[num_canales] = {ST_IDLE};
    static T_out_8bit registro_salida_global = 0;

    static TL_data fir_shift_reg[num_canales][num_coefs] = {0};
    static TL_acum shift_reg_det[num_canales][num_coefs_det] = {0};

#pragma HLS ARRAY_PARTITION variable=media_movil complete dim=1
#pragma HLS ARRAY_PARTITION variable=estado_actual complete dim=1
#pragma HLS ARRAY_PARTITION variable=fir_shift_reg complete dim=1
#pragma HLS ARRAY_PARTITION variable=shift_reg_det complete dim=1

    // Procesamiento solo si la palabra actual es de un canal (1..8)
    if (pkt_count >= 1 && pkt_count <= num_canales) {
        int channel = pkt_count - 1;   // 0..7

        TL_data adc_value = aux_input_raw;  // ya es signed

        // Filtro FIR
        TL_acum acum_fir_local = 0;
    loop_fir:
        for (int i = num_coefs - 1; i >= 0; i--) {
#pragma HLS PIPELINE II=1
            TL_data val = (i == 0) ? adc_value : fir_shift_reg[channel][i - 1];
            fir_shift_reg[channel][i] = val;
            acum_fir_local += val * FIR_coefs[i];
        }

        // Detector de envolvente (rectificado + media móvil)
        TL_acum aux_input = (acum_fir_local < 0) ? (TL_acum)(-acum_fir_local)
                                                 : (TL_acum)acum_fir_local;
        Q_suma acum_det_local = 0;
    loop_det:
        for (int i = num_coefs_det - 1; i >= 0; i--) {
#pragma HLS PIPELINE II=1
            TL_acum val_d = (i == 0) ? aux_input : shift_reg_det[channel][i - 1];
            shift_reg_det[channel][i] = val_d;
            acum_det_local += val_d;
        }
        // División por num_coefs_det (256 -> >>8)
        media_movil[channel] = acum_det_local >> 8;

        // Umbralización con histéresis
        Q_suma   val_actual = media_movil[channel];
        Q_suma   u_alto     = umbral[channel];
        Q_suma   u_bajo     = u_alto - (Q_suma)MARGEN_HISTERESIS;
        t_estado st         = estado_actual[channel];

        if (st == ST_IDLE) {
            if (val_actual > u_alto) st = ST_ACTIVE;
        } else {
            if (val_actual < u_bajo) st = ST_IDLE;
        }
        estado_actual[channel] = st;

        // Actualización del bit correspondiente en el registro global
        ap_uint<8> mask = (ap_uint<8>)1 << channel;
        if (st == ST_ACTIVE)
            registro_salida_global |= mask;
        else
            registro_salida_global &= ~mask;
    }

    if (pkt_count == 8) {
        axis_out_t salida_axis;

        salida_axis.data = registro_salida_global;
        salida_axis.keep = -1; // 0xFF, todos los bytes de la palabra válidos
        salida_axis.last = 1;

        // El IP solo interactúa con el bus de salida una vez cada 9 llamadas
        data_out_stream.write(salida_axis);
    }
}
