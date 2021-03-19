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
int rotate = 0;          // Bildschirm im rotationsmodus betreiben, in ° (90er Schritte)

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
        "%s [-m waveform][-d][-l][-s][-h][-r][-p] device x y w h\n\n"
        "Beispiele: \n"
        "# cat data.raw | %s -m 2 -l -s /dev/sg0 0 0 50 50\n"
        "# %s -m 2 -l -s -p /root/test.raw /dev/sg0 0 0 50 50\n"
        "# %s -m 2 -f 255 -s /dev/sg0 0 0 50 50\n\n"
        "Optionen:\n"
        "   Device: Pfad zum USB-Gerät z.B. /dev/sg0\n"
        "   -m: Waveform\n"
        "   -d: Debug\n"
        "   -p: Pfad zu Datei\n"
        "   -r: Devicerotation (0, 90°)\n"
        "   -f: Fill: gewählte Bildfläche im Speicher mit Farbe 0-255 füllen \n"
        "   -l: Lade Input auf IT8951 Speicher\n"
        "   -i: Displayinformationen ausgeben\n"
        "   -s: IT8951 Speicher zeichnen \n"
        "   x y w h: Bildposition und grösse\n"
        "   Input via Pipe, 8Bit-Graustufen Bild\n\n",
        appName, appName);
}

/*
 * Spannung und Powerstate des IT8951 setzen
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
        0x00,
        0x00,
        0x00,
        0x01,
        pwr};

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
 * Bilddaten in IT8951 Speicher zeichnen
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
void loadImage(unsigned char *buffer, int x, int y, int w, int h) {
    // Gemäss Doku ist die Maximalgröse pro Paket 60KByte. Der Input muss entsprechend geteilt werden
    int offsetLines = 0;
    int lines = 60000 / w;

    while (offsetLines < h) {
        if (offsetLines + lines > h)
            lines = h - offsetLines;

        if (debug)
            printf("Sende Teilbild: x%d,y%d,w%d,h%d\n", x, y + offsetLines, w, lines);

        loadImageArea(x, y + offsetLines, w, lines, &buffer[offsetLines * w]);
        offsetLines += lines;
    }
}

/*
 * Bilddaten aus Pipe lesen
 */
unsigned char *readImageStream(int size) {
    unsigned char *inputBuffer = (unsigned char *)malloc(size);
    unsigned char *inputBufferPointer = inputBuffer;

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

    return inputBuffer;
}

/**
 * Bilddaten aus Datei lesen
 */
unsigned char *readFile(char *filename) {
    int size = 0;
    FILE *file = fopen(filename, "r");

    if (!file) {
        fputs("File error.\n", stderr);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    char *stream = (char *)malloc(size);

    if (!stream) {
        fclose(file);
        perror("Speicherfehler");
        exit(EXIT_FAILURE);
    }

    if (fread(stream, 1, size, file) != size) {
        fclose(file);
        perror("Lesefehler");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return stream;
}

/**
 * Bild transformieren (Bytereihenfolge ändern und rotieren)
 */
unsigned char *transformImage(unsigned char *buffer, int w, int h) {
    unsigned char *outputBuffer = (unsigned char *)malloc(w * h);
    unsigned char *bufferPointer = buffer;

    if (rotate == 0) {
        for (int line = h; line != 0; --line) {
            for (int pixel = w; pixel != 0; --pixel) {
                outputBuffer[line * w - pixel] = *bufferPointer;
                bufferPointer++;
            }
        }
    } else if (rotate == 90) {
        for (int pixel = 0; pixel != w; ++pixel) {
            for (int line = h; line != 0; --line) {
                outputBuffer[line * w - pixel] = *bufferPointer;
                bufferPointer++;
            }
        }
    }

    return outputBuffer;
}

/*
 * Einfarbige Bildfläche generieren
 */
unsigned char *emptyImage(int w, int h, int fillColor) {
    unsigned char *buffer = (unsigned char *)malloc(w * h);
    memset(buffer, fillColor, w * h);
    return buffer;
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
    int fill = 265;
    int path = 0;
    char *pathValue;

    while ((opt = getopt(argc, argv, "m:dlshir:f:p:")) != -1) {
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
            case 'f':
                fill = atoi(optarg);
                break;
            case 's':
                show = 1;
                break;
            case 'p':
                 printf("Given File: %s\n", optarg);
                path = 1;
                pathValue = optarg;
                break;
            case 'r':
                rotate = atoi(optarg);
                break;
            case 'h':
            default: {
                printHelp(argv[0]);
                return EXIT_SUCCESS;
            }
        }
    }

    if (!show && !load && !info && fill > 255)
        return EXIT_SUCCESS;

    init(argv[optind]);

    int x = atoi(argv[optind + 1]);
    int y = atoi(argv[optind + 2]);
    int w = atoi(argv[optind + 3]);
    int h = atoi(argv[optind + 4]);

    // Koordinaten korrigieren
    if (rotate == 0) {
        y = deviceinfo.height - y - h;
    } else if (rotate == 90) {
        int temp = h;
        h = w;
        w = temp;

        temp = x;
        x = deviceinfo.width - y - w;
        y = deviceinfo.height - temp - h;
    }

    // Bildpassung prüfen
    if (deviceinfo.width < x + w) {
        perror("Bildüberstand (breite)");
        exit(EXIT_FAILURE);
    }

    if (deviceinfo.height < y + h) {
        perror("Bildüberstand (höhe)");
        exit(EXIT_FAILURE);
    }

    // Befehl ausführen
    if (load) {
        unsigned char *streamPointer;

        if (path) {
            printf("Given File: %s\n", pathValue);
            streamPointer = readFile(pathValue);
        } else {
            streamPointer = readImageStream(w * h);
        }

        streamPointer = transformImage(streamPointer, w, h);
        loadImage(streamPointer, x, y, w, h);
    }
    if (fill < 256)
        loadImage(emptyImage(w, h, fill), x, y, w, h);
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