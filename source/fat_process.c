#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>

#include "stdint.h"
#include "fat_process.h"
#include "mbr_defs.h"
#include "fat_defs.h"
#include "doubly_linked_list.h"

#define SECTOR_SIZE (512)
#define SECTOR_SIZE_SHIFT (9)
#define SECTOR_TO_BYTE_OFFSET(x) (x << SECTOR_SIZE_SHIFT)

int process_image_file(char* szFilename, int nPatchFat)
{
    FILE*               pFile = 0;
    int                 nStatus;
    int32_t             lClusterSize = 0;
    int32_t             lClusterCount = 0;
    int32_t             lFileAllocationTableSize = 0;
    int32_t             lRootDirectoryEntryOffset = 0;

    uint32_t*           pFAT1_Buffer = 0;
    uint32_t*           pFAT2_Buffer = 0;
    fat_node *          pFatList = 0;
    fat_chain *         pFatChainList = 0;
    uint32_t            ulFatChainCount = 0;
    int                 nReturnValue = 0;

    // Open the file.
    pFile = fopen(szFilename, "rb");

    if (0 == pFile)
    {
        fprintf(stderr, "fopen() failed on file: '%s.\n", szFilename);
        nReturnValue = -1;
        goto exit;
    }

    nReturnValue = read_fs_config_data(
        pFile,
        &pFAT1_Buffer,
        &pFAT2_Buffer,
        &lClusterSize,
        &lClusterCount,
        &lFileAllocationTableSize,
        &lRootDirectoryEntryOffset);


    // Attempt to correct inconsistencies in the FAT tables (if requested).
    if (nPatchFat != 0)
    {
        nReturnValue = patch_file_allocation_tables(
            pFAT1_Buffer,
            pFAT2_Buffer,
            lFileAllocationTableSize,
            lClusterCount);
    }

    // Consolidate FAT tables & directories.
    ulFatChainCount = process_fat_entries(
        &pFatList,
        pFAT1_Buffer,
        lFileAllocationTableSize);

    // Process fat list to fat chain list.
    nReturnValue = process_fat_chains(
        &pFatChainList,
        ulFatChainCount,
        pFatList,
        lFileAllocationTableSize);

    // Process directories.
    nReturnValue = process_dir_entries(
        pFile,
        pFatList,
        pFAT1_Buffer,
        pFatChainList,
        ulFatChainCount,
        lClusterSize,
        lClusterCount,
        FAT_ROOT_DIR,
        lRootDirectoryEntryOffset);

    // Report Results.
    nReturnValue = report_fat_dir_entries(
        pFatChainList,
        ulFatChainCount);


exit:
    // Free the pFAT1_Buffer buffer.
    if (0 != pFAT1_Buffer)
    {
        free (pFAT1_Buffer);
        pFAT1_Buffer = 0;
    }

    // Free the pFAT2_Buffer buffer.
    if (0 != pFAT2_Buffer)
    {
        free (pFAT2_Buffer);
        pFAT2_Buffer = 0;
    }

    // Free the pFatList buffer.
    if (0 != pFatList)
    {
        free (pFatList);
        pFatList = 0;
    }

    // Free the pFatChainList buffer.
    if (0 != pFatChainList)
    {
        free (pFatChainList);
        pFatChainList = 0;
    }

    // Close the file.
    if (pFile != 0)
    {
        if(0 != (nStatus = fclose(pFile)))
        {
            fprintf(stderr, "fclose() failed (%d) on file: '%s.'\n",
                nStatus, szFilename);

            nReturnValue = -1;
        }
    }

    return nReturnValue;
}

int read_fs_config_data(
    FILE*               pFile,
    uint32_t**          ppFAT1_Buffer,
    uint32_t**          ppFAT2_Buffer,
    int32_t*            pClusterSize,
    int32_t*            pClusterCount,
    int32_t*            pFileAllocationTableSize,
    int32_t*            pRootDirectoryEntryOffset)
{
    int                 nStatus;
    __int64             llFileSize = 0;
    size_t              ulBytesRead = 0;
    size_t              ulBytesToRead = 0;

    int32_t             lClusterSize = 0;
    int32_t             lClusterCount = 0;
    int32_t             lPartition1Offset = 0;
    int32_t             lFileSystemInfoOffset = 0;
    int32_t             lFileAllocationTableSize = 0;
    int32_t             lFileAllocationTable1Offset = 0;
    int32_t             lFileAllocationTable2Offset = 0;
    int32_t             lRootDirectoryEntryOffset = 0;

    MASTER_BOOT_RECORD* pMBR_Buffer = 0;
    FAT32_BOOT_SECTOR*  pFAT_BootSectorBuffer = 0;
    FS_INFO_SECTOR*     pFS_InfoSectorBuffer = 0;
    uint32_t*           pFAT1_Buffer = 0;
    uint32_t*           pFAT2_Buffer = 0;
    int                 nReturnValue = 0;

    // Allocate buffers.
    pMBR_Buffer = malloc(sizeof(MASTER_BOOT_RECORD));
    pFAT_BootSectorBuffer = malloc(sizeof(FAT32_BOOT_SECTOR));
    pFS_InfoSectorBuffer = malloc(sizeof(FS_INFO_SECTOR));

    if ( (0 == pMBR_Buffer) ||
         (0 == pFAT_BootSectorBuffer) ||
         (0 == pFS_InfoSectorBuffer) )
    {
        fprintf(stderr, "allocations failed.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Set the file size.
    _fseeki64(pFile, 0, SEEK_END);
    llFileSize = _ftelli64(pFile);

    // Read the Master Boot Record.
    nStatus = fseek (pFile, 0, SEEK_SET);
    if (0 != nStatus)
    {
        fprintf(stderr, "fseek() failed to seek to requested position.\n");
        nReturnValue = -1;
        goto exit;
    }

    ulBytesToRead = sizeof(MASTER_BOOT_RECORD);
    ulBytesRead = fread(pMBR_Buffer,1,ulBytesToRead,pFile);
    if(ulBytesToRead != ulBytesRead)
    {
        fprintf(stderr, "fread() failed to read the request amount.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Seek to the FAT Boot Sector.
    lPartition1Offset =
        SECTOR_TO_BYTE_OFFSET(pMBR_Buffer->partEntry1.startSectorOffset);

    nStatus = fseek (pFile, lPartition1Offset, SEEK_SET);
    if (0 != nStatus)
    {
        fprintf(stderr, "fseek() failed to seek to requested position.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Read the FAT Boot Sector.
    ulBytesToRead = sizeof(FAT32_BOOT_SECTOR);
    ulBytesRead = fread(pFAT_BootSectorBuffer,1,ulBytesToRead,pFile);

    if(ulBytesToRead != ulBytesRead)
    {
        fprintf(stderr, "fread() failed to read the request amount.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Seek to the File System Info Sector.
    lFileSystemInfoOffset = (lPartition1Offset +
        SECTOR_TO_BYTE_OFFSET(pFAT_BootSectorBuffer->fsInformationSector));

    nStatus = fseek (pFile, lFileSystemInfoOffset, SEEK_SET);
    if (0 != nStatus)
    {
        fprintf(stderr, "fseek() failed to seek to requested position.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Read the File System Info Sector.
    ulBytesToRead = sizeof(FS_INFO_SECTOR);
    ulBytesRead = fread(pFS_InfoSectorBuffer,1,ulBytesToRead,pFile);

    if(ulBytesToRead != ulBytesRead)
    {
        fprintf(stderr, "fread() failed to read the request amount.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Calculate Cluster Size.
    lClusterSize = (pFAT_BootSectorBuffer->bytesPerSector *
        pFAT_BootSectorBuffer->sectorsPerCluster);

    // Allocate the File Allocation Table Buffers & Root Dir Buffer.
    lFileAllocationTableSize =
        SECTOR_TO_BYTE_OFFSET(pFAT_BootSectorBuffer->sectorsPerFat32);

    pFAT1_Buffer = malloc(lFileAllocationTableSize);
    pFAT2_Buffer = malloc(lFileAllocationTableSize);

    if ( (0 == pFAT1_Buffer) || (0 == pFAT2_Buffer) )
    {
        fprintf(stderr, "allocations failed.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Seek to the File Allocation Table 1.
    lFileAllocationTable1Offset = lPartition1Offset +
        SECTOR_TO_BYTE_OFFSET(pFAT_BootSectorBuffer->sectorsBeforeFat);

    nStatus = fseek (pFile, lFileAllocationTable1Offset, SEEK_SET);
    if (0 != nStatus)
    {
        fprintf(stderr, "fseek() failed to seek to requested position.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Read the File Allocation Table 1.
    ulBytesToRead = lFileAllocationTableSize;
    ulBytesRead = fread(pFAT1_Buffer,1,ulBytesToRead,pFile);

    if(ulBytesToRead != ulBytesRead)
    {
        fprintf(stderr, "fread() failed to read the request amount.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Seek to the File Allocation Table 2.
    lFileAllocationTable2Offset =lFileAllocationTable1Offset +
        lFileAllocationTableSize;

    // Calculate Root Dir offset.
    lRootDirectoryEntryOffset = lFileAllocationTable2Offset +
        lFileAllocationTableSize;

    nStatus = fseek (pFile, lFileAllocationTable2Offset, SEEK_SET);
    if (0 != nStatus)
    {
        fprintf(stderr, "fseek() failed to seek to requested position.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Read the File Allocation Table 2.
    ulBytesToRead = lFileAllocationTableSize;
    ulBytesRead = fread(pFAT2_Buffer,1,ulBytesToRead,pFile);

    if(ulBytesToRead != ulBytesRead)
    {
        fprintf(stderr, "fread() failed to read the request amount.\n");
        nReturnValue = -1;
        goto exit;
    }

    // Calculate number of clusters.
    ulBytesToRead = lClusterSize;
    lClusterCount = ((pFAT_BootSectorBuffer->totalSectors2 - lRootDirectoryEntryOffset) /
        lClusterSize);

    // Set return variables.
    *ppFAT1_Buffer = pFAT1_Buffer;
    *ppFAT2_Buffer = pFAT2_Buffer;
    *pClusterSize = lClusterSize;
    *pClusterCount = lClusterCount;
    *pFileAllocationTableSize = lFileAllocationTableSize;
    *pRootDirectoryEntryOffset = lRootDirectoryEntryOffset;

exit:
    // Free the pMBR_Buffer buffer.
    if (0 != pMBR_Buffer)
    {
        free (pMBR_Buffer);
        pMBR_Buffer = 0;
    }

    // Free the pFAT_BootSectorBuffer buffer.
    if (0 != pFAT_BootSectorBuffer)
    {
        free (pFAT_BootSectorBuffer);
        pFAT_BootSectorBuffer = 0;
    }

    // Free the pFS_InfoSectorBuffer buffer.
    if (0 != pFS_InfoSectorBuffer)
    {
        free (pFS_InfoSectorBuffer);
        pFS_InfoSectorBuffer = 0;
    }

    return nReturnValue;
}

/* return (0 => unmodified, 1 => modified) */
int patch_file_allocation_tables(
    uint32_t* pFat1Buffer,
    uint32_t* pFat2Buffer,
    uint32_t ulFatSize,
    uint32_t ulClusterCount)
{
    int      nFAT_Modified = 0;
    int      nFATs_ValueMatch = 0;
    int      nFAT1_ValueValid = 0;
    int      nFAT2_ValueValid = 0;

    uint32_t ulIndex = 0;
    uint32_t ulExpectedValue = 0;
    uint32_t ulFatEntries = (ulFatSize >> 2);

    /* Compare the FAT tables against each other, one entry at a time. */
    for (ulIndex = 2; ulIndex < ulFatEntries; ++ulIndex)
    {
        /* Check for invalid entry in FAT1. */
        nFAT1_ValueValid =
            (pFat1Buffer[ulIndex] <= ulClusterCount) ? 1 :
            ((pFat1Buffer[ulIndex] >= 0x0FFFFFF0) && (pFat1Buffer[ulIndex] <= 0x0FFFFFFF)) ? 1 : 0;

        /* Check for invalid entry in FAT2. */
        nFAT2_ValueValid =
            (pFat2Buffer[ulIndex] <= ulClusterCount) ? 1 :
            ((pFat2Buffer[ulIndex] >= 0x0FFFFFF0) && (pFat2Buffer[ulIndex] <= 0x0FFFFFFF)) ? 1 : 0;

        /* Calculate the expected value. */
        ulExpectedValue = (ulIndex + 1);

        /* Check if the values match. */
        nFATs_ValueMatch = (pFat1Buffer[ulIndex] == pFat2Buffer[ulIndex]) ? 1 : 0;

        /* FAT1 == FAT2, both valid. */
        if ((nFAT1_ValueValid == 1) && (nFAT2_ValueValid == 1) && (nFATs_ValueMatch == 1))
        {
            /* Nothing to do. */
        }

        /* FAT1 <> FAT2, both invalid. */
        else if ((nFAT1_ValueValid == 0) && (nFAT2_ValueValid == 0) && (nFATs_ValueMatch == 0))
        {
            fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  Both invalid, nothing to do.\n",
                ulIndex,
                pFat1Buffer[ulIndex],
                pFat2Buffer[ulIndex]);
        }

        /* Both are valid, but mismatch. */
        else if ((nFAT1_ValueValid == 1) && (nFAT2_ValueValid == 1) && (nFATs_ValueMatch == 0))
        {
            /* FAT1 matches expected value.  Use it. */
            if ((ulExpectedValue == pFat1Buffer[ulIndex]) || (0x0ffffff8 == pFat1Buffer[ulIndex]))
            {
                fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  FAT1 matches expected, replacing FAT2 value.\n",
                    ulIndex,
                    pFat1Buffer[ulIndex],
                    pFat2Buffer[ulIndex]);

                pFat2Buffer[ulIndex] = pFat1Buffer[ulIndex];
                nFAT_Modified = 1;
            }

            /* FAT2 matches expected value.  Use it. */
            else if ((ulExpectedValue == pFat2Buffer[ulIndex]) || (0x0ffffff8 == pFat2Buffer[ulIndex]))
            {
                fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  FAT2 matches expected, replacing FAT1 value.\n",
                    ulIndex,
                    pFat1Buffer[ulIndex],
                    pFat2Buffer[ulIndex]);

                pFat1Buffer[ulIndex] = pFat2Buffer[ulIndex];
                nFAT_Modified = 1;
            }

            /* FAT1 <> FAT2 <> expected value, default to FAT1. */
            else
            {
                fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  Defaulting to FAT1 value.\n",
                    ulIndex,
                    pFat1Buffer[ulIndex],
                    pFat2Buffer[ulIndex]);

                pFat2Buffer[ulIndex] = pFat1Buffer[ulIndex];
                nFAT_Modified = 1;
            }
        }

        /* FAT1 Invalid, FAT2 Valid. */
        else if ((nFAT1_ValueValid == 0) && (nFAT2_ValueValid == 1))
        {
            fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  FAT1 Invalid, Using FAT2.\n",
                ulIndex,
                pFat1Buffer[ulIndex],
                pFat2Buffer[ulIndex]);

            pFat1Buffer[ulIndex] = pFat2Buffer[ulIndex];
            nFAT_Modified = 1;
        }

        /* FAT1 Valid, FAT2 Invalid. */
        else if ((nFAT1_ValueValid == 1) && (nFAT2_ValueValid == 0))
        {
            fprintf(stderr, "FAT entry (%lu): FAT1 (%8.8X) <> FAT2 (%8.8X).  FAT2 Invalid, Using FAT1.\n",
                ulIndex,
                pFat1Buffer[ulIndex],
                pFat2Buffer[ulIndex]);

            pFat2Buffer[ulIndex] = pFat1Buffer[ulIndex];
            nFAT_Modified = 1;
        }
    }

    return nFAT_Modified;
}

/* returns the number of FAT chains found. */
uint32_t process_fat_entries(
    fat_node ** ppFatList,
    uint32_t *  pFAT1_Buffer,
    uint32_t    ulFatSize)
{
    uint32_t ulFatEntries = (ulFatSize >> 2);
    uint32_t ulClusterNext = 0;
    uint32_t ulClusterCurr = 0;
    uint32_t ulClusterChainCount = 0;

    fat_node * pFatList;

    /* Allocate Fat Node List. */
    pFatList = malloc(ulFatEntries * sizeof(fat_node));
    *ppFatList = pFatList;

    memset(pFatList, 0x00, ulFatEntries * sizeof(fat_node));

    /* Compare the FAT tables against each other, one entry at a time. */
    for (ulClusterCurr = 2; ulClusterCurr < ulFatEntries; ++ulClusterCurr)
    {
        /* Set value of next entry. */
        ulClusterNext = pFAT1_Buffer[ulClusterCurr];

        /* Set value of Fat Entry. */
        pFatList[ulClusterCurr].cluster = ulClusterCurr;
        pFatList[ulClusterCurr].value   = ulClusterNext;

        /* Unallocated Cluster. */
        if (pFatList[ulClusterCurr].value == 0)
        {
            /* Do nothing. */
        }

        /* Allocated Cluster - Chain. */
        else if (pFatList[ulClusterCurr].value < ulFatEntries)
        {
            pFatList[ulClusterCurr].next = &pFatList[ulClusterNext];
            pFatList[ulClusterNext].prev = &pFatList[ulClusterCurr];
        }

        /* Allocated Cluster - End of Chain. */
        else if((pFatList[ulClusterCurr].value == FAT_EOC) ||
                (pFatList[ulClusterCurr].value == FAT_EOF))
        {
            /* Complete chain found. */
            ++ulClusterChainCount;
        }
    }

    return ulClusterChainCount;
}

int process_fat_chains(
    fat_chain ** ppChainList,
    uint32_t     ulChainCount,
    fat_node   * pFatList,
    uint32_t     ulFatSize)
{
    int nReturnValue = 0;
    uint32_t ulFatChainIndex = 0;
    uint32_t ulFatEntryIndex = 0;
    uint32_t ulFatEntryCount = (ulFatSize >> 2);
    fat_chain * pChainList = 0;
    fat_chain * pChainNode = 0;
    fat_node * pFatNode = 0;

    *ppChainList = pChainList = malloc(ulChainCount * sizeof(fat_chain));
    memset(pChainList, 0x00, ulChainCount * sizeof(fat_chain));

    for (ulFatEntryIndex = 0; ulFatEntryIndex < ulFatEntryCount; ulFatEntryIndex++)
    {
        if ((pFatList[ulFatEntryIndex].value == FAT_EOC) ||
            (pFatList[ulFatEntryIndex].value == FAT_EOF))
        {
            pFatNode = &pFatList[ulFatEntryIndex];
            pChainNode = &pChainList[ulFatChainIndex];

            /* set chain tail */
            pChainNode->tail = pFatNode;

            /* find chain head */
            while (pFatNode->prev != 0)
                pFatNode = pFatNode->prev;

            /* set chain head */
            pChainNode->head = pFatNode;

            /* increment chain index */
            ++ulFatChainIndex;
        }
    }

    return nReturnValue;
}

int process_dir_entries(
    FILE*       pFile,
    fat_node *  pFatList,
    uint32_t *  pFatBuffer,
    fat_chain * pFatChainList,
    uint32_t    ulFatChainCount,
    uint32_t    ulClusterSize,
    uint32_t    ulClusterCount,
    uint32_t    ulDirClusterIndex,
    uint32_t    ulRootDirOffset)
{
    int nReturnValue = 0;
    int nStatus = 0;
    FAT32_DIR_ENTRY* pDirectoryBuffer = 0;
    FAT32_DIR_ENTRY* pDirEntry = 0;
    size_t           ulBytesRead = 0;
    size_t           ulBytesToRead = 0;
    uint32_t         ulIndex = 0;
    uint32_t         ulBlockIndex = 0;
    uint32_t         ulBlockCount = 0;
    uint32_t         ulEntryClusterIndex = 0;
    __int64          llDirOffset;
    uint32_t         ulDirsPerCluster;
    fat_node*        pFatNode = 0;
    fat_chain*       pChainNode = 0;

    // Calculate the number of directory entries per cluster.
    ulDirsPerCluster = (ulClusterSize / sizeof(FAT32_DIR_ENTRY));

    // Allocate buffer for directory.
    pDirectoryBuffer = malloc(ulClusterSize);
    if (pDirectoryBuffer == 0)
    {
        fprintf(stderr, "allocations failed.\n");
        nReturnValue = -1;
        goto exit;
    }

    if (ulDirClusterIndex >= ulClusterCount)
    {
        fprintf(stderr, "invalid directory entry.\n");
        nReturnValue = -1;
        goto exit;
    }

    /* Determine number of clusters in dir. */
    pFatNode = &pFatList[ulDirClusterIndex];
    ulBlockCount = 1;

    while (pFatNode->next != 0)
    {
        ++ulBlockCount;
        pFatNode = pFatNode->next;
    }

    for (ulBlockIndex = 0; ulBlockIndex < ulBlockCount; ulBlockIndex++)
    {
        llDirOffset = ulRootDirOffset + (ulDirClusterIndex + ulBlockIndex - 2) * ulClusterSize;

        // Seek to the Directory Table.
        nStatus = _fseeki64 (pFile, llDirOffset, SEEK_SET);
        if (0 != nStatus)
        {
            fprintf(stderr, "fseek() failed to seek to requested position.\n");
            nReturnValue = -1;
            goto exit;
        }

        // Read the Directory Entry.
        ulBytesToRead = ulClusterSize;
        ulBytesRead = fread(pDirectoryBuffer,1,ulBytesToRead,pFile);

        if (ulBytesToRead != ulBytesRead)
        {
            fprintf(stderr, "fread() failed to read the request amount.\n");
            nReturnValue = -1;
            goto exit;
        }

        // Process the entries in the directory buffer.
        for (ulIndex = 0; ulIndex < ulDirsPerCluster; ulIndex++)
        {
            pDirEntry = &pDirectoryBuffer[ulIndex];
            ulEntryClusterIndex  = (pDirEntry->clusterAddressHigh << 16);
            ulEntryClusterIndex |= (pDirEntry->clusterAddressLow << 0);

            /* Per FAT32 spec:  "0x00 => Entry is available and no subsequenty entry is in use. */
            if (pDirEntry->dosFilename[0] == 0)
            {
#ifdef skip
                /* set index to count to break out of second level loop. */
                ulBlockIndex = ulBlockCount;
                break;
#endif
            }
            else
            {

               /* Find the chain associated with the directory entry. */
               pChainNode = find_fat_chain(
                   pFatChainList,
                   ulFatChainCount,
                   ulEntryClusterIndex);

               /* Populate the chain fields. */
               if ((pChainNode != 0) && (pChainNode->populated == 0))
               {
                   memcpy(pChainNode->filename, pDirEntry->dosFilename, sizeof(pChainNode->filename));
                   memcpy(pChainNode->extension, pDirEntry->dosExtension, sizeof(pChainNode->extension));

                   pChainNode->attributes = pDirEntry->fileAttributes;
                   pChainNode->filesize = pDirEntry->fileSizeBytes;
                   pChainNode->timestamp.time_ms = pDirEntry->creationTimeMs;
                   pChainNode->timestamp.time.value = pDirEntry->creationTimeHMS;
                   pChainNode->timestamp.date.value = pDirEntry->creationDate;

                   pChainNode->populated = 1;
               }

               process_dir_entry(pChainNode, pDirEntry);

               /* if entry is subdirectory (exlude dot entry), recursively process. */
               if ((0 != (pDirEntry->fileAttributes & FILE_ATTRIB_DIR)) &&
                   (pDirEntry->dosFilename[0] != FILE_DOT_ENTRY))
               {
                   process_dir_entries(
                       pFile,
                       pFatList,
                       pFatBuffer,
                       pFatChainList,
                       ulFatChainCount,
                       ulClusterSize,
                       ulClusterCount,
                       ulEntryClusterIndex,
                       ulRootDirOffset);
               }
            }
        }
    }

exit:
    // Free the pRootDirectoryBuffer buffer.
    if (0 != pDirectoryBuffer)
    {
        free (pDirectoryBuffer);
        pDirectoryBuffer = 0;
    }

    return nReturnValue;
}

int report_fat_dir_entries(
    fat_chain * pFatChainList,
    uint32_t    ulFatChainCount)
{
    int nReturnValue = 0;
    uint32_t ulFatChainIndex = 0;

    for (ulFatChainIndex = 0; ulFatChainIndex < ulFatChainCount; ++ulFatChainIndex)
    {
        report_fat_chain(&pFatChainList[ulFatChainIndex]);
    }

    return nReturnValue;
}

int report_fat_chain(fat_chain* pFatChainNode)
{
    int nReturnValue = 0;

    fprintf(stdout, "%8.8s %3.3s : %10dB : %2.2X : %04d-%02d-%02d %02d:%02d:%02d.%03d @ ",
        pFatChainNode->filename,
        pFatChainNode->extension,
        pFatChainNode->filesize,
        pFatChainNode->attributes,
        pFatChainNode->timestamp.date.decode.year + 1980,
        pFatChainNode->timestamp.date.decode.month,
        pFatChainNode->timestamp.date.decode.day,
        pFatChainNode->timestamp.time.decode.hour,
        pFatChainNode->timestamp.time.decode.min,
        pFatChainNode->timestamp.time.decode.sec,
        pFatChainNode->timestamp.time_ms);

    report_fat_alloc(pFatChainNode);

    return nReturnValue;
}

int report_fat_alloc(fat_chain * pFatChainNode)
{
    int nReturnValue = 0;
    fat_node * pFatNode = pFatChainNode->head;
    uint32_t ulBlockStart = 0;
    uint32_t ulBlockEnd = 0;

    /* forward track while reporting chain. */
    while (pFatNode->next != 0) {
        if (pFatNode->cluster + 1 == pFatNode->next->cluster)
        {
            if (ulBlockStart == 0)
                ulBlockStart = pFatNode->cluster;

            ulBlockEnd = pFatNode->value;
        }

        else
        {
            fprintf(stdout, "[%8.8X->%8.8X],",
                (ulBlockStart == 0) ? pFatNode->cluster : ulBlockStart,
                (ulBlockEnd == 0)   ? pFatNode->cluster : ulBlockEnd);

            /* reset contiguous block variables */
            ulBlockStart = 0;
            ulBlockEnd = 0;
        }

        pFatNode = pFatNode->next;
    }

    fprintf(stdout, "[%8.8X->%8.8X]\n",
        (ulBlockStart == 0) ? pFatNode->cluster : ulBlockStart,
        (ulBlockEnd == 0)   ? pFatNode->cluster : ulBlockEnd);

    return nReturnValue;
}

int process_dir_entry(
    fat_chain * pFatChain,
    FAT32_DIR_ENTRY* pDirEntry)
{
    int nReturnValue = 0;
    uint32_t  ulClusterIndex = 0;

    ulClusterIndex |= (pDirEntry->clusterAddressHigh << 16);
    ulClusterIndex |= (pDirEntry->clusterAddressLow << 0);

    /* Print filename + extension. */
    fprintf(stdout, "%8.8s %3.3s: ", pDirEntry->dosFilename, pDirEntry->dosExtension);

    /* Don't process deleted files. */
    if (pDirEntry->dosFilename[0] == FILE_DEL_ENTRY)
    {
        fprintf(stdout, "----DELETED---- Value: %8.8X\n", ulClusterIndex);
    }

    /* Directory Entry refers to invalid FAT Chain. */
    else if (pFatChain == 0)
    {
        fprintf(stdout, "----INVALID---- \n");
    }

    /* Directory Entry referes to valid FAT Chain. */
    else
    {
        report_fat_alloc(pFatChain);
    }

    return nReturnValue;
}

fat_chain* find_fat_chain(
    fat_chain* pFatChainList,
    uint32_t   ulFatChainLength,
    uint32_t   ulClusterStart)
{
    fat_chain* pSearchChain = 0;
    fat_chain* pResultChain = 0;
    uint32_t ulFatChainIndex = 0;

    for (ulFatChainIndex = 0; ulFatChainIndex < ulFatChainLength; ++ulFatChainIndex)
    {
        pSearchChain = &pFatChainList[ulFatChainIndex];
        if (pSearchChain->head->cluster == ulClusterStart)
        {
            pResultChain = pSearchChain;
            break;
        }
    }

    return pResultChain;
}

fat_chain* find_fat_chain_by_name(
    char* szFilename,
    char* szExtension,
    fat_chain* pFatChainList,
    uint32_t   ulFatChainCount)
{
    fat_chain* pResultChain = 0;
    uint32_t ulFatChainIndex = 0;
    char* szNodeFilename = 0;
    char* szNodeExtension = 0;
    char  szSearchFilename[8];
    char  szSearchExtension[3];

    uint32_t ulIndex = 0;

    strncpy(&szSearchFilename[0], szFilename, sizeof(szSearchFilename));
    strncpy(&szSearchExtension[0], szExtension, sizeof(szSearchExtension));

    for (ulIndex = 0; ulIndex < sizeof(szSearchFilename); ++ulIndex)
        if (szSearchFilename[ulIndex] == 0x00)
            szSearchFilename[ulIndex] = 0x20;

    for (ulIndex = 0; ulIndex < sizeof(szSearchExtension); ++ulIndex)
        if (szSearchExtension[ulIndex] == 0x00)
            szSearchExtension[ulIndex] = 0x20;

    for (ulFatChainIndex = 0; ulFatChainIndex < ulFatChainCount; ++ulFatChainIndex)
    {
        szNodeFilename = (char*)pFatChainList[ulFatChainIndex].filename;
        szNodeExtension = (char*)pFatChainList[ulFatChainIndex].extension;

        if ((0 == memcmp(szNodeExtension, szNodeExtension, sizeof(szNodeExtension))) &&
            (0 == memcmp(szNodeFilename, szSearchFilename, sizeof(szSearchFilename))))
        {
            pResultChain = &pFatChainList[ulFatChainIndex];
        }
    }

    return pResultChain;
}

void dump_buffer(
    uint32_t* pBuffer,
    uint32_t  ulBufferLength)
{
    uint32_t ulBufferIndex = 0;

    for (ulBufferIndex = 0; ulBufferIndex < ulBufferLength / sizeof(uint32_t); ulBufferIndex++)
    {
        fprintf(stdout, "%8.8X ", pBuffer[ulBufferIndex]);
    }
}

int extract_contents(
    FILE*      pFile,
    char*      szFilename,
    char*      szExtension,
    uint32_t   ulBytes,
    fat_chain* pFatChainList,
    uint32_t   ulFatChainCount,
    uint32_t   ulRootDirOffset,
    uint32_t   ulClusterSize)
{
    int nReturnValue = 0;
    fat_chain* pChainNode = 0;
    fat_node* pFatNode = 0;
    __int64 llFileOffset = 0;
    void* pFileBuffer = 0;
    uint32_t ulBytesToRead;
    size_t   ulBytesRead;
    size_t   ulBytesRemaining = ulBytes;

    pChainNode = find_fat_chain_by_name(
        szFilename,
        szExtension,
        pFatChainList,
        ulFatChainCount);

    if (pChainNode == 0)
    {
        nReturnValue = -1;
    }
    else
    {
        pFileBuffer = malloc(ulClusterSize);
        pFatNode = pChainNode->head;

        if (ulBytesRemaining == 0)
        {
            ulBytesRemaining = pChainNode->filesize;
        }

        fprintf(stdout, "Contents of file \"%8.8s %3.3s\":\n",
            pChainNode->filename, pChainNode->extension);

        while ((pFatNode != 0) && (ulBytesRemaining > 0))
        {
            ulBytesToRead = (ulClusterSize > ulBytesRemaining) ?
                (uint32_t)ulBytesRemaining : ulClusterSize;

            llFileOffset = ulRootDirOffset + (pFatNode->cluster - 2) * ulClusterSize;

            _fseeki64(pFile, llFileOffset, SEEK_SET);
            ulBytesRead = fread(pFileBuffer, 1, ulBytesToRead, pFile);
            ulBytesRemaining -= ulBytesRead;

            dump_buffer(pFileBuffer, ulBytesToRead);

            pFatNode = pFatNode->next;
        }
        fprintf(stdout, "\n");

        free (pFileBuffer);
    }

    return nReturnValue;

}

