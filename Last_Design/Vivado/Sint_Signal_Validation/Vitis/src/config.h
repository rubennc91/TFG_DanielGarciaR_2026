#include "ads1298r.h"

#define SAMPLING_RATE_TO_KHZ(value) \
    ((value) == CONFIG1_HIGH_RES_1k_SPS ? 1 : \
     (value) == CONFIG1_HIGH_RES_2k_SPS ? 2 : \
     (value) == CONFIG1_HIGH_RES_4k_SPS ? 4 : \
     (value) == CONFIG1_HIGH_RES_8k_SPS ? 8 : 0)

#define GAIN_TO_VALUE(value) \
    ((value) == CHNSET_GAIN_1X ? 1 : \
     (value) == CHNSET_GAIN_2X ? 2 : \
     (value) == CHNSET_GAIN_4X ? 4 : \
     (value) == CHNSET_GAIN_6X ? 6 : \
     (value) == CHNSET_GAIN_8X ? 8 : \
	 (value) == CHNSET_GAIN_12X ? 12 : 0)

#define INPUT_TO_STRING(input) \
	((input) == CHNSET_ELECTRODE_INPUT ? "ELECTRODE" : \
	 (input) == CHNSET_SHORTED ? "SHORTED" : \
	 (input) == CHNSET_TEST_SIGNAL ? "TEST SIGNAL" : "UNKNOWN" )

#define SAMPLING_RATE CONFIG1_HIGH_RES_8k_SPS

#define CH1_GAIN	CHNSET_GAIN_6X
#define CH1_INPUT	CHNSET_TEST_SIGNAL

#define CH2_GAIN	CHNSET_GAIN_6X
#define CH2_INPUT	CHNSET_TEST_SIGNAL

#define CH3_GAIN	CHNSET_GAIN_6X
#define CH3_INPUT	CHNSET_TEST_SIGNAL

#define CH4_GAIN	CHNSET_GAIN_6X
#define CH4_INPUT	CHNSET_TEST_SIGNAL

#define CH5_GAIN	CHNSET_GAIN_6X
#define CH5_INPUT	CHNSET_TEST_SIGNAL

#define CH6_GAIN	CHNSET_GAIN_6X
#define CH6_INPUT	CHNSET_TEST_SIGNAL

#define CH7_GAIN	CHNSET_GAIN_6X
#define CH7_INPUT	CHNSET_TEST_SIGNAL

#define CH8_GAIN	CHNSET_GAIN_6X
#define CH8_INPUT	CHNSET_TEST_SIGNAL
