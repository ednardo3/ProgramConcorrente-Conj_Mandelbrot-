
#define _POSIX_C_SOURCE 200809L //para função time

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define XMIN (-2.0)
#define XMAX (1.0)
#define YMIN (-1.5)
#define YMAX (1.5)
#define MAX_ITER 1000

static int parse_int(const char *text, const char *name, int *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed < 2 || parsed > INT32_MAX) {
        fprintf(stderr, "Erro: %s deve ser um inteiro maior ou igual a 2.\n",
                name);
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static void calculate(uint16_t *image, int width, int height)
{
    const double dx = (XMAX - XMIN) / (width - 1);
    const double dy = (YMAX - YMIN) / (height - 1);

    for (int row = 0; row < height; row++) {
        const double cy = YMIN + row * dy;

        for (int col = 0; col < width; col++) {
            const double cx = XMIN + col * dx;
            double zx = 0.0;
            double zy = 0.0;
            int iter = 0;

            while (zx * zx + zy * zy <= 4.0 && iter < MAX_ITER) {
                const double next_zx = zx * zx - zy * zy + cx;
                const double next_zy = 2.0 * zx * zy + cy;

                zx = next_zx;
                zy = next_zy;
                iter++;
            }

            image[(size_t)row * width + col] = (uint16_t)iter;
        }
    }
}

static double elapsed(const struct timespec *start,
                      const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

static uint64_t checksum(const uint16_t *image, size_t count)
{
    uint64_t sum = 0;

    for (size_t i = 0; i < count; i++) {
        sum += image[i];
    }

    return sum;
}

int main(int argc, char *argv[])
{
    int width;
    int height;
    size_t pixels;
    uint16_t *image;
    struct timespec start;
    struct timespec end;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <largura> <altura>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!parse_int(argv[1], "largura", &width) ||
        !parse_int(argv[2], "altura", &height)) {
        return EXIT_FAILURE;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        fprintf(stderr, "Erro: dimensoes excedem o limite suportado.\n");
        return EXIT_FAILURE;
    }

    pixels = (size_t)width * height;

    if (pixels > SIZE_MAX / sizeof(*image)) {
        fprintf(stderr, "Erro: imagem excede o limite de memoria.\n");
        return EXIT_FAILURE;
    }

    image = malloc(pixels * sizeof(*image));
    if (image == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        free(image);
        return EXIT_FAILURE;
    }

    calculate(image, width, height);

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        free(image);
        return EXIT_FAILURE;
    }

    printf("largura=%d altura=%d max_iter=%d\n",
           width, height, MAX_ITER);
    printf("tempo_calculo_segundos=%.9f\n", elapsed(&start, &end));
    printf("checksum=%" PRIu64 "\n", checksum(image, pixels));

    free(image);
    return EXIT_SUCCESS;
}
