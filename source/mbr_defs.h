#ifndef __MBR_DEFS_H_HEADER__
#define __MBR_DEFS_H_HEADER__

/* Kernel include(s) */
#include "stdint.h"

/* Pack all structures together as tightly as possible. */
#pragma pack(1)

/**
 * Standard Partition Table Entry Definition.
 */
typedef struct partitionTableEntry
{
   uint8_t      bootIndicator;          /* [00-00] Boot indicator byte (80h = active, otherwise 00h). */
   uint8_t      startDiskHead;          /* [01-01] Starting head (or side) of partition. */
   uint16_t     startCylinderAndSector; /* [02-03] Starting cylinder (10 bits) and sector (6 bits). */
   uint8_t      systemIndicator;        /* [04-04] System indicator byte. */
   uint8_t      endDiskHead;            /* [05-05] Ending head (or side) of partition. */
   uint16_t     endCylinderAndSector;   /* [06-07] Ending cylinder (10 bits) and sector (6 bits). */
   uint32_t     startSectorOffset;      /* [08-11] Relative sector offset of partition (in sectors). */
   uint32_t     numSectors;             /* [12-15] Total number of sectors in partition. */
} PARTITION_TABLE_ENTRY;

/**
 * Standard Master Boot Record Definition.
 */
typedef struct masterBootRecord
{
   uint8_t                 unused[446];      /* [000-445] Unused space. */
   PARTITION_TABLE_ENTRY   partEntry1;       /* [446-461] Partition Table Entry 1. */
   PARTITION_TABLE_ENTRY   partEntry2;       /* [462-477] Partition Table Entry 2. */
   PARTITION_TABLE_ENTRY   partEntry3;       /* [478-493] Partition Table Entry 3. */
   PARTITION_TABLE_ENTRY   partEntry4;       /* [494-509] Partition Table Entry 4. */
   uint8_t                 bootSignature[2]; /* [510-511] Boot sector signature (0x55 0xAA). */

} MASTER_BOOT_RECORD;

/* Stop packing structures. */
#pragma pack()

#endif /* __MBR_DEFS_H_HEADER__ */

