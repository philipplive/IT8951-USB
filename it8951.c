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
int fd = 0;              // Open File Descriptor

/*
 * Init und Display-Infos laden
 */
void it8951Init(const char *inputname) {
    fd = open(inputname, O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        perror("SCSI Gerät konnte nicht geöffnet werden");
        exit(EXIT_FAILURE);
    }

    sg_io_hdr_t ioHdr;
    memset(&ioHdr, 0, sizeof(sg_io_hdr_t));

    uint8_t cmd[11] = {
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

    uint8_t deviceinfoResult[112];

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
}

/*
 * Spannung und Powerstate des IT8951 setzen
 */
void it8951PmicSet(int pwr, int vcom) {
    uint8_t cmd[16] = {
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
    ioHdr.dxfer_direction = SG_DXFER_FROM_DEV;
    ioHdr.dxfer_len = 0;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO pmicSet Fehler");
        exit(EXIT_FAILURE);
    }

    return;
}

/*
 * Bilddaten in IT8951 Speicher zeichnen
 */
void it8951DisplayArea(int x, int y, int w, int h, int mode) {
    uint8_t cmd[16] = {
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

    uint8_t *buffer = (uint8_t *)malloc(sizeof(IT8951_displayArea));
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

    return;
}

/*
 * Bildinhalt übertragn in IT8951 Speicher
 */
void it8951LoadImageArea(int x, int y, int w, int h, uint8_t *data) {
    int length = w * h;
    IT8951_area area;
    memset(&area, 0, sizeof(IT8951_area));

    area.address = deviceinfo.memaddr;
    area.x = be32toh(x);
    area.y = be32toh(y);
    area.w = be32toh(w);
    area.h = be32toh(h);

    uint8_t cmd[7] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        IT8951_CMD_LOAD_IMG_AREA};

    uint8_t *buffer = (uint8_t *)malloc(length + sizeof(IT8951_area));
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

    return;
}

/**
 * Daten direkt in Speicher schreiben (fast)
 */
void it8951WriteMemory(uint32_t address, uint8_t *data, int size) {
    uint8_t cmd[16] = {
        IT8951_CMD_CUSTOMER,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        IT8951_CMD_FAST_WRITE_MEM};

    uint32_t *addr;
    addr = (uint32_t *)&cmd[2];
    *addr = htobe32(address);

    uint16_t *len;
    len = (uint16_t *)&cmd[7];
    *len = htobe16(size);

    sg_io_hdr_t ioHdr;
    ioHdr.interface_id = 'S';
    ioHdr.cmd_len = sizeof(cmd);
    ioHdr.cmdp = cmd;
    ioHdr.dxfer_direction = SG_DXFER_TO_DEV;
    ioHdr.dxfer_len = size;
    ioHdr.dxferp = data;

    if (ioctl(fd, SG_IO, &ioHdr) < 0) {
        perror("SG_IO Schreibfehler");
        exit(EXIT_FAILURE);
    }

    return;
}

/*
 * Bilddaten übertragen
 */
void it8951LoadImage(uint8_t *buffer, int x, int y, int w, int h) {
    // Gemäss Doku ist die Maximalgröse pro Paket beschränkt. Der Stream muss entsprechend aufgeteilt werden
    int lines = 60000 / w;  // Zu übertragende Zeilenanzahl pro Block
    int offsetLines = 0;    // Bereits übertragene Zeilenanzahl
    int useFastMode = 0;    // Falls möglich, direkt in Speicher schreiben (funktioniert nicht 100%)

    // Fast oder normaler Modus
    if (x == 0 && w == deviceinfo.width && useFastMode) {
        printf("FAST Mode\r\n");
        lines = 32767 / w;

        while (offsetLines < h) {
            if (offsetLines + lines > h)
                lines = h - offsetLines;

            it8951WriteMemory(htobe32(deviceinfo.memaddr) + ((offsetLines + deviceinfo.height - h) * w), &buffer[offsetLines * w], w * lines);
            offsetLines += lines;
        }
    } else {
        while (offsetLines < h) {
            if (offsetLines + lines > h)
                lines = h - offsetLines;

            it8951LoadImageArea(x, y + offsetLines, w, lines, &buffer[offsetLines * w]);
            offsetLines += lines;
        }
    }
}

/**
 * Bild transformieren (Bytereihenfolge ändern und rotieren)
 */
uint8_t *it8951TransformImage(uint8_t *buffer, int w, int h, int rotate) {
    uint8_t *outputBuffer = (uint8_t *)malloc(w * h);
    uint8_t *bufferPointer = buffer;

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

uint32_t it8951GetWidth() {
    return deviceinfo.width;
}

uint32_t it8951GetHeight() {
    return deviceinfo.height;
}