#ifndef IT8951_H
#define IT8951_H

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

#define WAVEFORM_MODE_0 0 // INIT:	Auf Weiss zurücksetzen / Clear (2000ms)
#define WAVEFORM_MODE_1 1 // DU: 	None-Flash, aber nur für Wechsel von Grau auf Weiss/Schwarz!
#define WAVEFORM_MODE_2 2 // GC16:	Optimal für Bilder (450ms)
#define WAVEFORM_MODE_3 3 // GL16:		
#define WAVEFORM_MODE_4 4 // GLR16:
#define WAVEFORM_MODE_5 5 // GLD16:	
#define WAVEFORM_MODE_6 6 // A2:	Nur S/W, None-Flash, sehr schnell (120ms)
#define WAVEFORM_MODE_7 7 // DU4:	

void it8951Init(const char *inputname);
int it8951PmicSet(int pwr, int vcom);
int it8951DisplayArea(int x, int y, int w, int h, int mode);
int it8951LoadImageArea(int x, int y, int w, int h, u_int8_t *data);
u_int8_t *it8951TransformImage(u_int8_t *buffer, int w, int h,int rotate);
void it8951LoadImage(u_int8_t *buffer, int x, int y, int w, int h);
uint32_t it8951GetWidth();
uint32_t it8951GetHeight();

#endif