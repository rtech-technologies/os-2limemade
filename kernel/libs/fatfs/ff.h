#ifndef FF_DEFINED
#define FF_DEFINED

#include <stdint.h>

typedef char TCHAR;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint32_t FSIZE_t;

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
    BYTE fs_type;
    BYTE drv;
    /* ... minimal fields ... */
} FATFS;

typedef struct {
    FATFS* obj;
    DWORD fptr;
    FSIZE_t fsize;
    /* ... minimal fields ... */
} FIL;

typedef struct {
    FATFS* obj;
    /* ... minimal fields ... */
} DIR;

typedef struct {
    FSIZE_t fsize;
    WORD fdate;
    WORD ftime;
    BYTE fattrib;
    TCHAR fname[256];
} FILINFO;

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt);
FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close(FIL* fp);
FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br);
FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw);
FRESULT f_opendir(DIR* dp, const TCHAR* path);
FRESULT f_readdir(DIR* dp, FILINFO* fno);

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

#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_ALWAYS 0x08

#endif
