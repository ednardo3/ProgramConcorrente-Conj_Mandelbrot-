#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define XMIN (-2.0)
#define XMAX (1.0)
#define YMIN (-1.5)
#define YMAX (1.5)
#define MAX_ITER 1000
#define MAX_THREADS 32

typedef struct {
    uint16_t *image;
    int width;
    int height;
    int first_row;
    int last_row;
} Task;

/* Converte um argumento textual em inteiro dentro do intervalo informado. */
static int parse_int(const char *text, const char *name, int min, int max,
                     int *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed < min || parsed > max) {
        fprintf(stderr, "Erro: %s deve estar entre %d e %d.\n",
                name, min, max);
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

/* Calcula os pixels pertencentes ao intervalo de linhas da thread. */
static void *calculate(void *arg)
{
    Task *task = arg;
    const double dx = (XMAX - XMIN) / (task->width - 1);
    const double dy = (YMAX - YMIN) / (task->height - 1);

    for (int row = task->first_row; row < task->last_row; row++) {
        const double cy = YMIN + row * dy;

        for (int col = 0; col < task->width; col++) {
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

            task->image[(size_t)row * task->width + col] =
                (uint16_t)iter;
        }
    }

    return NULL;
}

/* Retorna o intervalo, em segundos, entre duas leituras do relógio */
static double elapsed(const struct timespec *start,
                      const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

/* Produz uma soma simples dos valores da imagem para validação rápida. */
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
    int thread_count;
    size_t pixels;
    uint16_t *image;
    pthread_t *threads;
    Task *tasks;
    struct timespec start;
    struct timespec end;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s <largura> <altura> <threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!parse_int(argv[1], "largura", 2, INT32_MAX, &width) ||
        !parse_int(argv[2], "altura", 2, INT32_MAX, &height) ||
        !parse_int(argv[3], "threads", 1, MAX_THREADS, &thread_count)) {
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
    threads = malloc((size_t)thread_count * sizeof(*threads));
    tasks = malloc((size_t)thread_count * sizeof(*tasks));

    if (image == NULL || threads == NULL || tasks == NULL) {
        perror("malloc");
        free(image);
        free(threads);
        free(tasks);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        free(image);
        free(threads);
        free(tasks);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < thread_count; i++) {
        tasks[i] = (Task) {
            .image = image,
            .width = width,
            .height = height,
            .first_row = i * height / thread_count,
            .last_row = (i + 1) * height / thread_count,
        };

        const int rc = pthread_create(&threads[i], NULL, calculate, &tasks[i]);

        if (rc != 0) {
            errno = rc;
            perror("pthread_create");

            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }

            free(image);
            free(threads);
            free(tasks);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < thread_count; i++) {
        const int rc = pthread_join(threads[i], NULL);

        if (rc != 0) {
            errno = rc;
            perror("pthread_join");
            free(image);
            free(threads);
            free(tasks);
            return EXIT_FAILURE;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        free(image);
        free(threads);
        free(tasks);
        return EXIT_FAILURE;
    }

    printf("largura=%d altura=%d threads=%d max_iter=%d\n",
           width, height, thread_count, MAX_ITER);
    printf("tempo_calculo_segundos=%.9f\n", elapsed(&start, &end));
    printf("checksum=%" PRIu64 "\n", checksum(image, pixels));

    free(image);
    free(threads);
    free(tasks);

    return EXIT_SUCCESS;
}