#ifndef STD_INT_H
#define STD_INT_H

typedef signed char             int8_t;
typedef short int               int16_t;
typedef int                     int32_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
#ifdef _MSC_VER
typedef signed __int64          int64_t;
typedef unsigned __int64        uint64_t;
#endif
#endif
