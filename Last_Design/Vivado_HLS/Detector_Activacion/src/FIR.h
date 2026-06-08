/**************************************************************
· file   FIR.h
· author Daniel García
· date   2026-02-07
· description:
    this header file contains parameters for high-level synthetization of a FIR filter
*************************************************************/
#ifndef FIR_H
#define FIR_H

#include "config.h"
#include <ap_axi_sdata.h>   // necesario para axis_out_t
#include "hls_stream.h"

void FIR_filter(
    hls::stream<TL_data_raw>& data_in_stream,
    hls::stream<axis_out_t>& data_out_stream,
    Q_suma umbral[num_canales]
);

#endif
