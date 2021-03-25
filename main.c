#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "it8951.h"

int rotate = 0;          // Bildschirm im rotationsmodus betreiben, in ° (90er Schritte)

/*
 * Hilfe
 */
void printHelp(const char *appName) {
    printf(
        "\n"
        "%s [-m waveform][-f][-l][-s][-h][-r][-p] device x y w h\n\n"
        "Beispiele: \n"
        "# cat data.raw | %s -m 2 -l -s /dev/sg0 0 0 50 50\n"
        "# %s -m 2 -l -s -p /root/test.raw /dev/sg0 0 0 50 50\n"
        "# %s -m 2 -f 255 -s /dev/sg0 0 0 50 50\n\n"
        "Optionen:\n"
        "   Device: Pfad zum USB-Gerät z.B. /dev/sg0\n"
        "   -m: Waveform\n"
        "   -p: Pfad zum File\n"
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
 * Bilddaten aus Pipe lesen
 */
u_int8_t *readImageStream(int size) {
    u_int8_t *inputBuffer = (u_int8_t *)malloc(size);
    u_int8_t *inputBufferPointer = inputBuffer;

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
u_int8_t *readFile(char *filename) {
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

/*
 * Einfarbige Bildfläche generieren
 */
u_int8_t *emptyImage(int w, int h, int fillColor) {
    u_int8_t *buffer = (u_int8_t *)malloc(w * h);
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

    it8951Init(argv[optind]);

    int x = atoi(argv[optind + 1]);
    int y = atoi(argv[optind + 2]);
    int w = atoi(argv[optind + 3]);
    int h = atoi(argv[optind + 4]);

    // Koordinaten korrigieren
    if (rotate == 0) {
        y = it8951GetHeight() - y - h;
    } else if (rotate == 90) {
        int temp = h;
        h = w;
        w = temp;

        temp = x;
        x = it8951GetWidth() - y - w;
        y = it8951GetHeight() - temp - h;
    }

    // Bildpassung prüfen
    if (it8951GetWidth() < x + w) {
        perror("Bildüberstand (breite)");
        exit(EXIT_FAILURE);
    }

    if (it8951GetHeight() < y + h) {
        perror("Bildüberstand (höhe)");
        exit(EXIT_FAILURE);
    }

    // Befehl ausführen
    if (load) {
        u_int8_t *streamPointer;

        if (path) {
            printf("Given File: %s\n", pathValue);
            streamPointer = readFile(pathValue);
        } else {
            streamPointer = readImageStream(w * h);
        }

        streamPointer = it8951TransformImage(streamPointer, w, h,rotate);
        it8951LoadImage(streamPointer, x, y, w, h);
    }
    if (fill < 256)
        it8951LoadImage(emptyImage(w, h, fill), x, y, w, h);
    if (show)
        it8951DisplayArea(x, y, w, h, mode);
    if (info) {
        printf(
            "Displayinformationen:\n"
            "w: %d\n"
            "h: %d\n",
            it8951GetWidth(), it8951GetHeight());
    }

    // Display auf Standby
    it8951PmicSet(0, 0);

    return EXIT_SUCCESS;
}