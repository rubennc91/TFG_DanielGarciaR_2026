/**************************************************************
· file   FIR_filter.h
· author Daniel García
· date   2026-04-10
· description:
	this header file contains parameters for high-level synthetization of a FIR filter
*************************************************************/

#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include "config.h"
#include "hls_stream.h"


void FIR_filter_tfm(
    hls::stream<TL_data_raw>& data_in_stream,
    hls::stream<axis_out_t>& data_out_stream
);

#endif
