#include "c64_oscar.h"
#include "jtxt.h"

#ifdef JTXT_MAGICDESK_CRT
#pragma code(mcode)
#pragma data(mdata)
#endif

// This file can be used for additional text mode specific functions if needed
// Currently most text mode functions are in jtxt.c