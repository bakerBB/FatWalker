#ifndef __FAT_PROCESS_H_HEADER__
#define __FAT_PROCESS_H_HEADER__

#include "stdint.h"
#include "fat_defs.h"

#define FAT_EOC         (0x0FFFFFF8)
#define FAT_EOF         (0x0FFFFFFF)
#define FAT_ROOT_DIR    (2)
#define FILE_ATTRIB_DIR (0x10)
#define FILE_DOT_ENTRY  (0x2E)
#define FILE_DEL_ENTRY  (0xE5)

typedef struct FAT_NODE {
    struct FAT_NODE * next;
    struct FAT_NODE * prev;
    uint32_t          cluster;
    uint32_t          value;
} fat_node;

typedef struct FAT_CHAIN {
    struct FAT_NODE * head;
    struct FAT_NODE * tail;
    unsigned char filename[8];
    unsigned char extension[3];
    uint32_t      filesize;
    uint8_t       attributes;

    struct {
        uint8_t time_ms;

        union {
            uint16_t value;

            struct {
                uint32_t hour : 5;
                uint32_t min  : 6;
                uint32_t sec  : 5;
            } decode;
        } time;

        union {
            uint16_t value;
            struct {
                uint32_t year : 7;
                uint32_t month : 4;
                uint32_t day : 5;
            } decode;
        } date;
    } timestamp;

    uint8_t populated;
} fat_chain;

fat_chain* find_fat_chain(
    fat_chain* pFatChainList,
    uint32_t   ulFatChainLength,
    uint32_t   ulClusterStart);

int process_image_file(
    char* szFilename,
    int   nPatch);

int read_fs_config_data(
    FILE*               pFile,
    uint32_t**          ppFAT1_Buffer,
    uint32_t**          ppFAT2_Buffer,
    int32_t*            pClusterSize,
    int32_t*            pClusterCount,
    int32_t*            pFileAllocationTableSize,
    int32_t*            pRootDirectoryEntryOffset);

int patch_file_allocation_tables(
    uint32_t* pFat1Buffer,
    uint32_t* pFat2Buffer,
    uint32_t ulFatSize,
    uint32_t ulClusterCount);

/* returns number of fat chains found. */
uint32_t process_fat_entries(
    fat_node ** pFatList,
    uint32_t *  pFAT1_Buffer,
    uint32_t    ulFatSize);

int process_fat_chains(
    fat_chain ** ppChainList,
    uint32_t     ulChainCount,
    fat_node   * pFatList,
    uint32_t     ulFatSize);

int process_dir_entries(
    FILE*       pFile,
    fat_node *  pFatList,
    uint32_t *  pFatBuffer,
    fat_chain * pFatChainList,
    uint32_t    ulFatChainCount,
    uint32_t    ulClusterSize,
    uint32_t    ulClusterCount,
    uint32_t    ulClusterIndex,
    uint32_t    ulRootDirOffset);

int process_dir_entry(
    fat_chain * pFatChain,
    FAT32_DIR_ENTRY* pDirEntry);

int report_fat_alloc(
    fat_chain * pFatChainNode);

int report_fat_dir_entries(
    fat_chain * pFatChainList,
    uint32_t    ulFatChainCount);

int report_fat_chain(
    fat_chain* pFatChainNode);

void dump_buffer(
    uint32_t* pBuffer,
    uint32_t  ulBufferLength);

int extract_contents(
    FILE*      pFile,
    char*      szFilename,
    char*      szExtension,
    uint32_t   ulBytes,
    fat_chain* pFatChainList,
    uint32_t   ulFatChainCount,
    uint32_t   ulRootDirOffset,
    uint32_t   ulClusterSize);

#endif /* __FAT_PROCESS_H_HEADER__ */
