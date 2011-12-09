#ifndef __FAT_DEFS_H_HEADER__
#define __FAT_DEFS_H_HEADER__

#include "stdint.h"

/* Pack all structures together as tightly as possible. */
#pragma pack(1)

/**
 * Standard ATA command set.
 */
typedef struct fatBootSector
{
   uint8_t    jumpAddress[3];      /* [000-002] Jump instruction. */
   char       oemName[8];          /* [003-010] OEM format name. */
   uint16_t   bytesPerSector;      /* [011-012] Bytes per sector.  Commonly 512. */
   uint8_t    sectorsPerCluster;   /* [013-013] Sectors per cluster. Must be power of two between 1 to 128. */
   uint16_t   sectorsBeforeFat;    /* [014-015] Number of sectors before the first FAT.  Usually 32 for FAT32. */
   uint8_t    numFatTables;        /* [016-016] Number of file allocation tables. Almost always 2. */
   uint16_t   maxRootDirEntries;   /* [017-018] Maximum number of root directory entries.  Should be 0 for FAT32. */
   uint16_t   totalSectors1;       /* [019-020] Total sectors (if zero, use 4 byte value at offset 0x20). */
   uint8_t    mediaDescriptor;     /* [021-021] Media descriptor. */
   uint16_t   sectorsPerFat16;     /* [022-023] Sectors per File Allocation Table (FAT12/FAT16 only). */
   uint16_t   sectorsPerTrack;     /* [024-025] Sectors per track (Only useful on disks with geometry). */
   uint16_t   sectorsPerHead;      /* [026-027] Number of heads (Only useful on disks with geometry). */
   uint32_t   numHiddenSectors;    /* [028-031] Number of hidden sectors preceding the partition that contains this FAT volume. */
   uint32_t   totalSectors2;       /* [032-035] Total sectors (if greater than 65535). */
   uint32_t   sectorsPerFat32;     /* [036-039] Sectors per file allocation table. */
   uint16_t   fat12Flags;          /* [040-041] FAT Flags (Only used by FAT12/16 volumes). */
   uint16_t   fatVersion;          /* [042-043] Version (Defined as 0). */
   uint32_t   rootDirStartCluster; /* [044-047] Cluster number of root directory start. */
   uint16_t   fsInformationSector; /* [048-049] Sector number of FS Information Sector. */
   uint16_t   backupBootSector;    /* [050-051] Sector number of a copy of this boot sector (0 if no backup copy exists). */
   uint8_t    reserved1[12];       /* [052-063] Reserved */
   uint8_t    driveNumber;         /* [064-064] Physical Drive Number. */
   uint8_t    reserved2;           /* [065-065] Reserved. */
   uint8_t    bootSignatureExt;    /* [066-066] Extended boot signature. */
   uint32_t   serialNumber;        /* [067-070] ID (Serial Number). */
   char       volumeLabel[11];     /* [071-081] Volume Label. */
   char       fsTypeId[8];         /* [082-089] FAT file system type ID tag.  Should be "FAT32   ". */
   uint8_t    osBootCode[420];     /* [090-509] Operating system boot code. */
   uint8_t    bootSignature[2];    /* [510-511] Boot sector signature (0x55 0xAA). */
} FAT32_BOOT_SECTOR;

typedef struct fsInfoSector
{
   char        fsInfoSectorSignature1[4]; /* [000-003] FS information sector signature (0x52 0x52 0x61 0x41 / "RRaA") */
   char        reserved[480];             /* [004-483] Reserved (byte values are 0x00) */
   char        fsInfoSectorSignature2[4]; /* [484-487] FS information sector signature (0x72 0x72 0x41 0x61 / "rrAa") */
   uint32_t    numFreeClusters;           /* [488-491] Number of free clusters on the drive, or -1 if unknown */
   uint32_t    newestClusterNum;          /* [492-495] Number of the most recently allocated cluster */
   char        reserved2[14];             /* [496-509] Reserved (byte values are 0x00) */
   char        fsInfoSectorSignature3[2]; /* [510-511] FS information sector signature (0x55 0xAA) */
} FS_INFO_SECTOR;

typedef struct fat32_dirEntry
{
    unsigned char dosFilename[8]; /* [00-07] DOS file name (padded with spaces) */
    unsigned char dosExtension[3]; /* [08-10] DOS file extension (padded with spaces) */
    uint8_t       fileAttributes; /* [11-11] File Attributes */
    uint8_t       reserved;
    uint8_t       creationTimeMs;
    uint16_t      creationTimeHMS;
    uint16_t      creationDate;
    uint16_t      accessDate;
    uint16_t      clusterAddressHigh;
    uint16_t      modificationTime;
    uint16_t      modificationDate;
    uint16_t      clusterAddressLow;
    uint32_t      fileSizeBytes;


} FAT32_DIR_ENTRY;

/* Stop packing structures. */
#pragma pack()

#endif /* __FAT_DEFS_H_HEADER__ */
