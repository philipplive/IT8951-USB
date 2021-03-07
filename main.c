#include <fcntl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "it8951.h"

IT8951_info deviceinfo;  // Display Infos
int debug = 0;           // Debugmodus
int fd = 0;              // Open File Descriptor

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
    unsigned char *inputBuffer = (unsigned char *)malloc(size);
    unsigned char *inputBufferPointer = inputBuffer;
    unsigned char *outputBuffer = (unsigned char *)malloc(size);

    // Pipeinput lesen
    int left = size;

    while (left > 0) {
        int current = read(STDIN_FILENO, inputBufferPointer, left);

        if (current < 0) {
            perror("Inputstream Error");
            exit(EXIT_FAILURE);
        } else if (current == 0) {
            fprintf(stderr, "Angegebene Bildgrösse passt nicht zum Inputstream\n");
            exit(EXIT_FAILURE);
        } else {
            left -= current;
            inputBufferPointer += current;
        }
    }

    // Bild transformieren (Bildstartpunkt des Streams korrigieren - von unten links nach oben links)
    inputBufferPointer = inputBuffer;

    for (int line = h; line != 0; --line) {
        for (int pixel = w; pixel != 0; --pixel) {
            outputBuffer[line * w - pixel] = *inputBufferPointer;
            inputBufferPointer++;
        }
    }

    // Gemäss Doku ist die Maximalgröse pro Paket 60KByte. Der Input muss entsprechend geteilt werden
    int offsetLines = 0;
    int lines = 60000 / w;

    while (offsetLines < h) {
        if (offsetLines + lines > h)
            lines = h - offsetLines;

        if (debug)
            printf("Sende Teilbild: x%d,y%d,w%d,h%d\n", x, y + offsetLines, w, lines);

        loadImageArea(x, y + offsetLines, w, lines, &outputBuffer[offsetLines * w]);
        offsetLines += lines;
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
    ioHdr.dxfer_len = sizeof(deviceinfo);
    ioHdr.dxferp = &deviceinfo;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO Geräteinfo fehlerhaft");
        exit(EXIT_FAILURE);
    }

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
        "%s [-m waveform][-d][-l][-s][-h] device x y w h\n\n"
        "Beispiel: \n"
        "cat data.raw | %s -m 2 -l -s /dev/sg0 0 0 50 50\n\n"
        "Optionen:\n"
        "   device: Pfad zum USB-Gerät z.B. /dev/sg0\n"
        "   -m: Waveform\n"
        "   -d: Debug\n"
        "   -l: Lade Input auf IT8951 Speicher\n"
        "   -i: Displayinformationen ausgeben\n"
        "   -s: Zeichne Display aus IT8951 Speicher \n"
        "   x y w h: Bildposition und grösse\n"
        "   Input via Pipe, 8Bit-Graustufen Bild\n\n",
        appName, appName);
}

/*
 * Main
 */
int main(int argc, char *argv[]) {
    int opt;
    int mode = WAVEFORM_MODE_2;
    int show = 0;
    int load = 0;
    int info = 0;

    while ((opt = getopt(argc, argv, "m:dlshi")) != -1) {
        switch (opt) {
            case 'm':
                mode = atoi(optarg);
                break;
            case 'd':
                debug = 1;
                break;
            case 'i':
                info = 1;
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

    if (!show && !load && !info)
        return EXIT_SUCCESS;

    init(argv[optind]);

    int x = atoi(argv[optind + 1]);
    int y = atoi(argv[optind + 2]);
    int w = atoi(argv[optind + 3]);
    int h = atoi(argv[optind + 4]);

    // Bildüberstand berechnen
    if (deviceinfo.width < x + w) {
        perror("Bildüberstand (breite)");
        exit(EXIT_FAILURE);
    }

    if (deviceinfo.height < y + h) {
        perror("Bildüberstand (höhe)");
        exit(EXIT_FAILURE);
    }
    
    // Y Achse korrigieren
    y = deviceinfo.height - y - h;

    // Befehl ausführen
    if (load)
        loadImage(x, y, w, h, mode);
    if (show)
        displayArea(x, y, w, h, mode);
    if (info) {
        printf(
            "Displayinformationen:\n"
            "w: %d\n"
            "h: %d\n",
            deviceinfo.width, deviceinfo.height);
    }

    // Display auf Standby
    pmicSet(0, 0);

    return EXIT_SUCCESS;
}