typedef struct {
    uint32_t stdCmdNum;
    uint32_t extCmdNum;
    uint32_t signature;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t updateMemaddr;
    uint32_t memaddr;
    uint32_t tempSegNum;
    uint32_t mode;
    uint32_t frameCount[8];
    uint32_t bufNum;
    uint32_t unused[9];
    void *command_table;
} IT8951_info;

typedef struct {
    uint32_t address;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} IT8951_area;

typedef struct {
    uint32_t address;
    uint32_t wavemode;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t waitReady;
} IT8951_displayArea;

#define IT8951_CMD_CUSTOMER 0xfe
#define IT8951_CMD_GET_SYS 0x80
#define IT8951_CMD_READ_MEM 0x81
#define IT8951_CMD_WRITE_MEM 0x82
#define IT8951_CMD_FAST_WRITE_MEM 0xa5
#define IT8951_CMD_displayArea 0x94
#define IT8951_CMD_SPI_ERASE 0x96
#define IT8951_CMD_SPI_READ 0x97
#define IT8951_CMD_SPI_WRITE 0x98
#define IT8951_CMD_LOAD_IMG_AREA 0xa2
#define IT8951_CMD_PMIC_CTRL 0xa3

#define WAVEFORM_MODE_0 0
#define WAVEFORM_MODE_1 1
#define WAVEFORM_MODE_2 2
#define WAVEFORM_MODE_6 2