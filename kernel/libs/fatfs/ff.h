#ifndef FF_DEFINED
#define FF_DEFINED

#include <stdint.h>
#include <stdbool.h>

typedef char TCHAR;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint32_t FSIZE_t;
typedef uint64_t LBA_t;

typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

typedef struct {
    BYTE  fs_type;      /* Fat type (0, FAT12, FAT16, FAT32) */
    BYTE  drv;          /* Physical drive number */
    BYTE  n_fats;       /* Number of FAT copies */
    BYTE  wflag;        /* win buffer dirty flag */
    BYTE  fsi_flag;     /* FSINFO dirty flag */
    WORD  id;           /* File system mount ID */
    WORD  n_rootdir;    /* Number of root directory entries (FAT12/16) */
    DWORD last_clst;    /* Last allocated cluster */
    DWORD free_clst;    /* Number of free clusters */
    DWORD n_fatent;     /* Number of FAT entries */
    DWORD fsize;        /* Sectors per FAT */
    LBA_t volbase;      /* Volume base sector */
    LBA_t fatbase;      /* FAT base sector */
    LBA_t dirbase;      /* Root directory base sector/cluster */
    LBA_t database;     /* Data base sector */
    DWORD winsect;      /* Current sector in win[] */
    BYTE  win[2048];    /* Disk access window for Directory/FAT/Boot */

    /* Sovereign Mechanical Context */
    uint16_t reserved_sectors;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint32_t root_cluster;
    LBA_t    data_lba;
    LBA_t    partition_lba;
    uint16_t sector_size;
    bool active;
    bool ro;            /* Read-only (Safe Mode) */
} FATFS;

typedef struct {
    FATFS*  obj;        /* Pointer to the hosting volume object */
    WORD    id;         /* Hosting volume mount ID */
    BYTE    flag;       /* File status flags */
    BYTE    err;        /* Error code */
    DWORD   fptr;       /* File read/write pointer */
    DWORD   fsize;      /* File size */
    DWORD   sclust;     /* File start cluster */
    DWORD   clust;      /* Current cluster */
    LBA_t   dsect;      /* Current data sector */
    uint32_t entry_lba;
    uint32_t entry_idx;
} FIL;

typedef struct {
    FATFS*  obj;        /* Pointer to the hosting volume object */
    WORD    id;         /* Hosting volume mount ID */
    DWORD   index;      /* Current directory index */
    DWORD   sclust;     /* Table start cluster */
    DWORD   clust;      /* Current cluster */
    LBA_t   sect;       /* Current sector */
    BYTE*   dir;        /* Pointer to the current SFN entry in win[] */
} DIR;

typedef struct {
    FSIZE_t fsize;      /* File size */
    WORD    fdate;      /* Last modified date */
    WORD    ftime;      /* Last modified time */
    BYTE    fattrib;    /* Attribute */
    TCHAR   fname[256]; /* File name */
} FILINFO;

/* File access control and open method flags */
#define FA_READ             0x01
#define FA_WRITE            0x02
#define FA_OPEN_EXISTING    0x00
#define FA_CREATE_ALWAYS    0x08
#define FA_CREATE_NEW       0x04
#define FA_OPEN_ALWAYS      0x10

/* FAT sub-type boundaries */
#define FS_FAT12    1
#define FS_FAT16    2
#define FS_FAT32    3

/* File attribute bits */
#define AM_RDO  0x01    /* Read only */
#define AM_HID  0x02    /* Hidden */
#define AM_SYS  0x04    /* System */
#define AM_VOL  0x08    /* Volume label */
#define AM_LFN  0x0F    /* LFN entry */
#define AM_DIR  0x10    /* Directory */
#define AM_ARC  0x20    /* Archive */

FRESULT f_mount(FATFS* fs, int drive);
FRESULT f_fdisk(int drive);
FRESULT f_mkfs(int drive);
FRESULT f_open(FATFS* fs, FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close(FIL* fp);
FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br);
FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw);
FRESULT f_opendir(FATFS* fs, DIR* dp, const TCHAR* path);
FRESULT f_readdir(DIR* dp, FILINFO* fno);
FRESULT f_mkdir(FATFS* fs, const TCHAR* path);
FRESULT f_unlink(FATFS* fs, const TCHAR* path);
FRESULT f_stat(FATFS* fs, const TCHAR* path, FILINFO* fno);
uint32_t f_get_next_cluster(FATFS* fs, uint32_t cluster);

/* diskio.h equivalent */
typedef BYTE DSTATUS;
typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count);
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count);

#endif
