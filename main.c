#include <fcntl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

// Debugmodus
int debug = 0;
// Open File Descriptor
int fd = 0;
// Display Infos
IT8951_info deviceinfo;

/*
 * Spannung und Powerstate setzen
 */
int pmicSet(int pwr, int vcom) {
    unsigned char cmd[16] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        IT8951_CMD_PMIC_CTRL,
        0x00, /* Vcom value [15:8] */
        0x00, /* Vcom value [7:0] */
        0x00, /* Set Vcom 0=no 1=yes */
        0x01, /* Set power 0=no 1=yes */
        pwr   /* Power value 0=off 1=on */
    };

    if (vcom) {
        cmd[7] = (vcom >> 8);
        cmd[8] = vcom;
        cmd[9] = 1;
    }

    sg_io_hdr_t ioHdr;

    memset(&ioHdr, 0, sizeof(sg_io_hdr_t));
    ioHdr.interface_id = 'S';
    ioHdr.cmd_len = sizeof(cmd);
    ioHdr.cmdp = cmd;
    ioHdr.dxfer_direction = SG_DXFER_TO_DEV;
    ioHdr.dxfer_len = 0;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO pmicSet Fehler");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/*
 * Übertragene Bilddaten anzeigen
 */
int displayArea(int x, int y, int w, int h, int mode) {
    unsigned char cmd[16] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        IT8951_CMD_displayArea};

    IT8951_displayArea area;
    memset(&area, 0, sizeof(IT8951_displayArea));

    area.address = deviceinfo.memaddr;
    area.x = be32toh(x);
    area.y = be32toh(y);
    area.w = be32toh(w);
    area.h = be32toh(h);
    area.waitReady = be32toh(1);
    area.wavemode = be32toh(mode);

    unsigned char *buffer = (unsigned char *)malloc(sizeof(IT8951_displayArea));
    memcpy(buffer, &area, sizeof(IT8951_displayArea));

    sg_io_hdr_t ioHdr;

    memset(&ioHdr, 0, sizeof(sg_io_hdr_t));
    ioHdr.interface_id = 'S';
    ioHdr.cmd_len = sizeof(cmd);
    ioHdr.cmdp = cmd;
    ioHdr.dxfer_direction = SG_DXFER_TO_DEV;
    ioHdr.dxfer_len = sizeof(IT8951_displayArea);
    ioHdr.dxferp = buffer;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO Anzeigefehler");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/*
 * Bildinhalt übertragn in IT8951 Speicher
 */
int loadImageArea(int x, int y, int w, int h, unsigned char *data) {
    int length = w * h;
    IT8951_area area;
    memset(&area, 0, sizeof(IT8951_area));

    area.address = deviceinfo.memaddr;
    area.x = be32toh(x);
    area.y = be32toh(y);
    area.w = be32toh(w);
    area.h = be32toh(h);

    unsigned char cmd[7] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        IT8951_CMD_LOAD_IMG_AREA};

    unsigned char *buffer = (unsigned char *)malloc(length + sizeof(IT8951_area));
    memcpy(buffer, &area, sizeof(IT8951_area));
    memcpy(&buffer[sizeof(IT8951_area)], data, length);

    sg_io_hdr_t ioHdr;
    memset(&ioHdr, 0, sizeof(sg_io_hdr_t));

    ioHdr.interface_id = 'S';
    ioHdr.cmd_len = sizeof(cmd);
    ioHdr.cmdp = cmd;
    ioHdr.dxfer_direction = SG_DXFER_TO_DEV;
    ioHdr.dxfer_len = length + sizeof(IT8951_area);
    ioHdr.dxferp = buffer;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO Bildladefehler");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/*
 * Bilddaten übertragen
 */
void loadImage(int x, int y, int w, int h, int mode) {
    int size = w * h;
    unsigned char *buffer = (unsigned char *)malloc(size);
    unsigned char *bufferPointer = buffer;
    size_t left = size;

    // Pipeinput lesen
    while (left > 0) {
        size_t current = read(STDIN_FILENO, bufferPointer, left);

        if (current < 0) {
            perror("Inputstream Error");
            exit(EXIT_FAILURE);
        } else if (current == 0) {
            fprintf(stderr, "Inputstream leer\n");
            exit(EXIT_FAILURE);
        } else {
            left -= current;
            bufferPointer += current;
        }
    }

    // Gemäss Doku ist die Maximalgröse pro Paket 60KByte. Der Input muss entsprechend geteilt werden
    int offset = 0;
    int lines = 60000 / w;

    while (offset < size) {
        if (offset / w + lines > h)
            lines = h - offset / w;

        if (debug)
            printf("Sende Teilbild: x%d,y%d,w%d,h%d\n", x, y + (offset / w), w, lines);

        loadImageArea(x, y + (offset / w), w, lines, &buffer[offset]);
        offset += lines * w;
    }
}

/*
 * Init und Display-Infos laden
 */
void init(const char *inputname) {
    fd = open(inputname, O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        perror("SCSI Gerät konnte nicht geöffnet werden");
        exit(EXIT_FAILURE);
    }

    IT8951_info *dev;
    dev = calloc(1, sizeof(*dev));
    sg_io_hdr_t ioHdr;
    memset(&ioHdr, 0, sizeof(sg_io_hdr_t));

    unsigned char cmd[11] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x38,
        0x39,
        0x35,
        0x31,
        IT8951_CMD_GET_SYS,
        0x00,
        0x01,
        0x00,
        0x02};

    unsigned char deviceinfoResult[112];

    ioHdr.interface_id = 'S';
    ioHdr.cmd_len = sizeof(cmd);
    ioHdr.cmdp = cmd;
    ioHdr.dxfer_direction = SG_DXFER_FROM_DEV;
    ioHdr.dxfer_len = sizeof(*dev);
    ioHdr.dxferp = dev;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO Geräteinfo fehlerhaft");
        exit(EXIT_FAILURE);
    }

    deviceinfo = *dev;
    deviceinfo.width = be32toh(deviceinfo.width);
    deviceinfo.height = be32toh(deviceinfo.height);

    if (debug)
        printf("Display gefunden, %dx%d\n", deviceinfo.width, deviceinfo.height);
}

/*
 * Hilfe
 */
void printHelp(const char *appName) {
    printf(
        "\n"
        "Beispiel: %s [-m waveform][-d][-l][-s] device x y w h\n\n"
        "Optionen:\n"
        "   device: Pfad zum USB-Gerät z.B. /dev/sg0\n"
        "   -m: Waveform\n"
        "   -d: Debug\n"
        "   -l: Lade Input auf IT8951 Speicher\n"
        "   -s: Zeichne Display aus IT8951 Speicher \n"
        "   x y w h: Bildposition und grösse\n"
        "   Input via Pipe, 8Bit-Graustufen Bild\n\n",
        appName);
}

/*
 * Main
 */
int main(int argc, char *argv[]) {
    int opt;
    int mode = WAVEFORM_MODE_2;
    int show = 0;
    int load = 0;

    while ((opt = getopt(argc, argv, "m:dls")) != -1) {
        switch (opt) {
            case 'm':
                mode = atoi(optarg);
                break;
            case 'd':
                debug = 1;
                break;
            case 'l':
                load = 1;
                break;
            case 's':
                show = 1;
                break;
            case 'h':
            default:
                printHelp(argv[0]);
        }
    }

    if (!show && !load)
        return EXIT_SUCCESS;

    int x = atoi(argv[optind + 1]);
    int y = atoi(argv[optind + 2]);
    int w = atoi(argv[optind + 3]);
    int h = atoi(argv[optind + 4]);

    init(argv[optind]);

    if (load)
        loadImage(x, y, w, h, mode);
    if (show)
        displayArea(x, y, w, h, mode);

    // Display auf Standby
    pmicSet(0, 0);

    return EXIT_SUCCESS;
}