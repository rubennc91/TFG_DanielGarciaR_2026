/**************************************************************
* file      FIR_filter_tb.cpp
* author    Daniel García
* date      2026-04-10
* description:
* Testbench for FIR filter. Simulates the removal of the Status Word.
*************************************************************/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "FIR_filter.h"
#include "config.h"
#include "hls_stream.h"
#include "ap_fixed.h"

int main() {

    FILE *fin;
    fin = fopen("datain_ch9_buenos.txt", "r");

    if (fin == NULL){
        printf("Error: No se ha encontrado el archivo datain_ch9_buenos.txt\n");
        return 1;
    }

    char dato_aux_in[128];
    unsigned long long temp_in = 0;

    hls::stream<TL_data_raw> data_in_stream;
    hls::stream<axis_out_t> salida_axis;

    printf("Iniciando simulación...\n");
    printf("Configuración: Saltando Status Word (n%%9==0) para sincronizar ráfaga de 8.\n");
    printf("----------------------------------------------------------------------\n");

    for (int n = 0; n < signal_length; n++) {

        // 1. Lectura del archivo
        if (fscanf(fin, "%s", dato_aux_in) == 1) {
            sscanf(dato_aux_in, "x\"%llx\"", &temp_in);
        } else {
            break;
        }


        if (n % 9 == 0) {
            // Ignorar STATUS
            continue;
        }

        TL_data_raw x_sample;
        x_sample.range(31, 0) = (ap_uint<32>)temp_in;

        // 2. Llamada al Kernel
        data_in_stream.write(x_sample);
        FIR_filter_tfm(data_in_stream, salida_axis);

        // 3. Procesamiento de Salida
        while (!salida_axis.empty()) {
            axis_out_t paquete_out = salida_axis.read();

            Q_suma envolvente_out;
            envolvente_out.range(31, 0) = paquete_out.data.range(31, 0);

            // Verificar cada 500 muestras o cuando haya un TLAST
            if (n % 500 == 0 || paquete_out.last == 1) {
                TL_data val_entrada_real;
                val_entrada_real.range(23, 0) = x_sample.range(23, 0);

                printf("n=%6d | In=%9.6f | Env_Out=%9.6f | TLAST=%d\n",
                        n,
                        val_entrada_real.to_double(),
                        envolvente_out.to_double(),
                        (int)paquete_out.last);
            }
        }
    }

    printf("-------------------------------------------\n");
    printf("SIMULACION FINALIZADA CON EXITO\n");

    fclose(fin);
    return 0;
}
