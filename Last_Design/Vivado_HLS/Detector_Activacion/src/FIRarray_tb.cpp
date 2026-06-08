/**************************************************************
· testbench_FIR.cpp
· author    Daniel García
· date      2026-04-22
· description:
    Testbench para el filtro FIR multicanal.
    Lee un archivo de texto con muestras de 24 bits (formato x"XXXXXXXX")
    y comprueba el comportamiento del filtro, mostrando la salida
    cuando se detectan cambios o cada cierto número de muestras.
*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FIR.h"
#include "config.h"
#include "hls_stream.h"
#include <ap_int.h>

int main() {
    // 1. APERTURA DEL ARCHIVO DE ENTRADA
    FILE *fin = fopen("datain_ch9_buenos.txt", "r");
    if (!fin) {
        printf("ERROR: No se pudo abrir 'datain_ch9_buenos.txt'.\n");
        printf("       Asegúrese de que el archivo existe en el directorio del proyecto.\n");
        return 1;
    }

    // 2. DECLARACIÓN DE STREAMS Y VARIABLES
    hls::stream<TL_data_raw>    data_in_stream;   // Stream de entrada (24 bits)
    hls::stream<axis_out_t> data_out_stream;  // Stream de salida (8 bits + TLAST)

    // Umbrales para cada canal (se pueden cargar desde archivo si se desea)
    Q_suma umbral[num_canales];
    for (int i = 0; i < num_canales; i++) {
        umbral[i] = (Q_suma)0.015;   // Valor de ejemplo, ajustar según la aplicación
    }

    // Variables para lectura del archivo
    char linea[128];
    unsigned long long temp_in;

    // Contador de muestras procesadas
    int n = 0;

    // Para control de impresión de cambios
    T_out_8bit last_salida = 0;
    int last_print_n = -1000;

    // 3. CABECERA DE LA SALIDA
    printf("\n");
    printf("SIMULACION DEL FILTRO FIR MULTICANAL\n");
    printf("====================================\n");
    printf("Archivo de entrada: datain_ch9_buenos.txt\n");
    printf("Umbrales: ");
    for (int i = 0; i < num_canales; i++) {
        printf("Ch%d=%.4f ", i, (double)umbral[i]);
    }
    printf("\n\n");

    printf("   n     |  salida  |\n");
    printf("---------|----------|\n");

    // 4. BUCLE PRINCIPAL DE PROCESAMIENTO
    #ifdef signal_length
        int max_samples = signal_length;
    #else
        int max_samples = 1000000;   // límite de seguridad
    #endif

        // 4. BUCLE PRINCIPAL DE PROCESAMIENTO
            for (n = 0; n < max_samples; n++) {
                if (fscanf(fin, "%s", linea) != 1) {
                    break;   // Fin de archivo
                }

                if (sscanf(linea, "x\"%llx\"", &temp_in) != 1) {
                    printf("ERROR: Formato incorrecto en la línea %d: %s\n", n+1, linea);
                    break;
                }

                TL_data_raw muestra_32bits = (ap_uint<32>)temp_in;

                int pkt_real = (temp_in >> 28) & 0xF;

                data_in_stream.write(muestra_32bits);

                FIR_filter(data_in_stream, data_out_stream, umbral);

                        if (pkt_real == 8) {
                            if (!data_out_stream.empty()) {
                                axis_out_t out_val = data_out_stream.read();
                                T_out_8bit salida_byte = out_val.data;
                                int tlast = out_val.last;

                                if (salida_byte != last_salida || n - last_print_n > 4500 || tlast == 0) {

                                    printf("%8d |", n);

                                    printf(" ");
                                    for (int bit = 7; bit >= 0; bit--) {
                                        printf("%d", (int)((salida_byte >> bit) & 1));
                                    }

                                    last_salida = salida_byte;
                                    last_print_n = n;
                                }
                            } else {
                                printf("ERROR CRÍTICO: Se alcanzó el fin de trama (pkt=8) pero el IP no generó salida en n=%d\n", n);
                            }
                        } else {
                            if (!data_out_stream.empty()) {
                                printf("ERROR DE PROTOCOLO: El IP inyectó un dato antes de tiempo en el bus (pkt=%d) en n=%d\n", pkt_real, n);
                                data_out_stream.read(); // Limpieza defensiva para evitar bucles infinitos en cosimulación
                            }
                        }
                    }

    // 5. FINALIZACIÓN
    fclose(fin);
    printf("\nProcesadas %d muestras.\n", n);
    printf("SIMULACION FINALIZADA CON EXITO.\n");
    return 0;
}
