#ifndef MNEMONICS_H
#define MNEMONICS_H

#if defined (__WIN32__)
/*
typedef unsigned __int8  byte;
typedef unsigned __int8  __uint8_t;
typedef unsigned __int16 __uint16_t;
typedef unsigned __int32 __uint32_t;
typedef unsigned __int64 __uint64_t;
*/
#endif

#define CAPTURE_CYCLE           1
#define CAPTURE_OPCODE          1
#define CAPTURE_VIRTUAL_ADDRESS 1
#define CAPTURE_ASCII_CODE      1
#define CAPTURE_EXCEPTION_STR   1
#define CAPTURE_MEM_DATA        1
	
#define PHYSICAL_ADDRESS_DATATYPE  __uint64_t
#define VIRTUAL_ADDRESS_DATATYPE   __uint64_t
#define CYCLES_DATATYPE            __uint32_t
#define OPCODE_DATATYPE            __uint32_t
#define MAX_DATA_WIDTH             __uint64_t

#endif
