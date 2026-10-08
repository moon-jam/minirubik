/* Host-side checks of the cross-compiled ELF on Ripes. */
#define _POSIX_C_SOURCE 200809L
#include "tables.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *ripes, *directory;
static unsigned char *image;
static size_t image_size, input_offset;
static FILE *results;
static unsigned completed, hard_count;
static uint64_t minimum = UINT64_MAX, maximum, total;
static char worst[15];

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "rv32-check: %s\n", message);
        exit(1);
    }
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    require(file != NULL, path);
    require(fseek(file, 0, SEEK_END) == 0, "file seek failed");
    long length = ftell(file);
    require(length >= 0, "file size failed");
    rewind(file);
    unsigned char *data = malloc((size_t) length + 1);
    require(data != NULL, "allocation failed");
    require(fread(data, 1, (size_t) length, file) == (size_t) length,
            "file read failed");
    require(fclose(file) == 0, "file close failed");
    data[length] = 0;
    *size = (size_t) length;
    return data;
}

static void save_file(const char *path, const void *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    require(file != NULL, path);
    require(fwrite(data, 1, size, file) == size, "file write failed");
    require(fclose(file) == 0, "file close failed");
}

static void path_for(char path[1024], const char *name)
{
    int length = snprintf(path, 1024, "%s/%s", directory, name);
    require(length > 0 && length < 1024, "output path too long");
}

static uint32_t word(size_t offset, unsigned width)
{
    require(offset <= image_size && width <= image_size - offset,
            "ELF offset out of range");
    uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= (uint32_t) image[offset + i] << (8 * i);
    return value;
}

static int rv32i(uint32_t instruction)
{
    unsigned op = instruction & 127, f3 = (instruction >> 12) & 7;
    unsigned f7 = instruction >> 25;
    switch (op) {
    case 0x37: case 0x17: case 0x6f: return 1;
    case 0x67: return f3 == 0;
    case 0x63: return f3 == 0 || f3 == 1 || f3 >= 4;
    case 0x03: return f3 <= 2 || f3 == 4 || f3 == 5;
    case 0x23: return f3 <= 2;
    case 0x13:
        if (f3 == 1) return f7 == 0;
        if (f3 == 5) return f7 == 0 || f7 == 32;
        return 1;
    case 0x33: return f7 == 0 || (f7 == 32 && (f3 == 0 || f3 == 5));
    case 0x0f: return f3 == 0 && (instruction >> 28) == 0;
    case 0x73: return instruction == 0x73 || instruction == 0x100073;
    default: return 0;
    }
}

static void inspect_elf(void)
{
    require(image_size >= 52 && memcmp(image, "\177ELF\1\1", 6) == 0,
            "expected little-endian ELF32");
    require(word(16, 2) == 2 && word(18, 2) == 243 && word(36, 4) == 0,
            "expected RV32I executable with no extension flags");
    size_t sections = word(32, 4);
    unsigned stride = word(46, 2), count = word(48, 2), inputs = 0;
    uint64_t data_bytes = 0, text_bytes = 0;
    require(stride == 40 && count != 0, "unexpected ELF section headers");
    for (unsigned i = 0; i < count; ++i) {
        size_t section = sections + (size_t) i * stride;
        unsigned type = word(section + 4, 4), flags = word(section + 8, 4);
        size_t offset = word(section + 16, 4), size = word(section + 20, 4);
        if (!(flags & 2)) continue;
        if (flags & 4) {
            require(type == 1 && size % 4 == 0, "unexpected text section");
            text_bytes += size;
            for (size_t j = 0; j < size; j += 4)
                require(rv32i(word(offset + j, 4)), "non-RV32I instruction");
        } else {
            data_bytes += size;
            if (type != 1 || (flags & 1)) continue;
            require(offset <= image_size && size <= image_size - offset,
                    "ELF data section out of range");
            for (size_t j = 0; j + 15 <= size; ++j)
                if (!memcmp(image + offset + j, "21345671111111", 15)) {
                    input_offset = offset + j;
                    ++inputs;
                }
        }
    }
    require(text_bytes != 0 && inputs == 1, "expected one read-only input string");
    require(data_bytes <= 131072, "static data exceeds 128 KiB");
    printf("RV32I audit passed: text=%" PRIu64 ", static data=%" PRIu64 " bytes\n",
           text_bytes, data_bytes);
}

static long long report_value(const char *text, const char *key)
{
    const char *value = strstr(text, key);
    require(value != NULL, "missing Ripes report field");
    value += strlen(key);
    while (isspace((unsigned char) *value)) ++value;
    char *end;
    long long result = strtoll(value, &end, 10);
    require(end != value, "invalid Ripes report value");
    return result;
}

static void run_case(const char *model, const char *input, int expected,
                     const char *kind)
{
    char elf[1024], report[1024], log[1024];
    path_for(elf, "current.elf");
    path_for(report, "current-report.txt");
    path_for(log, "current-output.txt");
    require(strlen(input) <= 14, "input too long for test slot");
    memset(image + input_offset, 0, 15);
    memcpy(image + input_offset, input, strlen(input));
    save_file(elf, image, image_size);
    /* Remove the old report so a failed run cannot reuse stale results. */
    require(unlink(report) == 0 || errno == ENOENT, "remove report failed");
    pid_t child = fork();
    require(child >= 0, "fork failed");
    if (child == 0) {
        FILE *output = fopen(log, "w");
        if (!output || dup2(fileno(output), STDOUT_FILENO) < 0 ||
            dup2(fileno(output), STDERR_FILENO) < 0) _exit(126);
        fclose(output);
        execl(ripes, ripes, "--mode", "cli", "--src", elf, "-t", "elf",
              "--proc", model, "--timeout", "300000", "--iret", "--cycles",
              "--exectime", "--runinfo", "--regs", "--output", report,
              (char *) NULL);
        _exit(127);
    }
    int status;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    require(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "Ripes process failed; inspect current-output.txt");
    size_t report_size, log_size;
    char *text = (char *) read_file(report, &report_size);
    char *output = (char *) read_file(log, &log_size);
    int returned = (int) report_value(text, "\nx8:");
    int exit_code = (int) report_value(output, "Program exited with code:");
    require(returned == expected && exit_code == (expected < 0),
            "incorrect result or program exit");
    require(strstr(text, "ISA extensions: \n") != NULL,
            "unexpected ISA extensions");
    const char *configured = strstr(text, "processor: ");
    require(configured && !strncmp(configured + 11, model, strlen(model)) &&
            configured[11 + strlen(model)] == '\n', "unexpected processor");
    long long retired = report_value(text, "===== instructions retired");
    long long cycles = report_value(text, "===== cycles");
    long long ms = report_value(text, "===== wall-clock model execution time (ms)");
    require(retired > 0 && cycles > 0 && ms >= 0, "invalid telemetry");
    fprintf(results, "%s,%s,%s,%d,%d,%lld,%lld,%lld,%d\n", model, input,
            kind, expected, returned, retired, cycles, ms, exit_code);
    require(fflush(results) == 0, "result write failed");
    if (!strcmp(kind, "hard")) {
        require(retired <= 50000000, "hard input exceeds instruction budget");
        if ((uint64_t) retired < minimum) minimum = (uint64_t) retired;
        if ((uint64_t) retired > maximum) {
            maximum = (uint64_t) retired;
            strcpy(worst, input);
            char path[1024]; path_for(path, "maximum-report.txt");
            save_file(path, text, report_size);
        }
        total += (uint64_t) retired;
        ++hard_count;
    }
    if (!strcmp(input, "21345671111111")) {
        char name[80], path[1024];
        snprintf(name, sizeof name, "%s-designated-report.txt", model);
        path_for(path, name); save_file(path, text, report_size);
        printf("%s designated: %lld retired instructions\n", model, retired);
    }
    if (++completed % 200 == 0) printf("%u cases passed\n", completed);
    free(text); free(output);
}

static void rank_string(unsigned rank, char input[15])
{
    uint8_t p[7], o[7];
    cube_unrank_permutation((uint16_t) (rank / 729), p);
    unsigned orientation = rank % 729, sum = 0;
    for (int i = 5; i >= 0; --i) {
        o[i] = (uint8_t) (orientation % 3);
        sum += o[i]; orientation /= 3;
    }
    o[6] = (uint8_t) ((3 - sum % 3) % 3);
    for (unsigned i = 0; i < 7; ++i) {
        input[i] = (char) ('1' + p[i]);
        input[i + 7] = (char) ('1' + o[i]);
    }
    input[14] = 0;
}

static void three_cases(const char *model)
{
    run_case(model, "12345671111111", 0, "solved");
    run_case(model, "25346712313322", 2, "short");
    run_case(model, "21345671111111", 11, "designated");
}

int main(int argc, char **argv)
{
    if (argc != 5 || (strcmp(argv[1], "full") && strcmp(argv[1], "quick"))) {
        fputs("usage: rv32-check full|quick RIPES ELF OUTPUT_DIRECTORY\n", stderr);
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    ripes = argv[2]; directory = argv[4];
    require(mkdir(directory, 0755) == 0 || errno == EEXIST, "create output directory");
    image = read_file(argv[3], &image_size);
    inspect_elf();
    char path[1024]; path_for(path, "results.csv");
    results = fopen(path, "w"); require(results != NULL, "open results failed");
    fputs("model,input,kind,expected,returned,iret,cycles,milliseconds,exit_code\n", results);
    if (!strcmp(argv[1], "full")) {
        tables_prepare(1);
        unsigned seen = 0;
        for (unsigned rank = 0; rank < STATES; ++rank) {
            unsigned depth = exact[rank];
            require(depth <= 11, "invalid reference depth");
            if (depth != 11 && (seen & (1U << depth))) continue;
            char input[15]; rank_string(rank, input);
            run_case("RV32_ISS", input, (int) depth,
                     depth == 11 ? "hard" : "depth-sample");
            seen |= 1U << depth;
        }
        require(hard_count == 2644 && seen == 4095, "incomplete reference coverage");
        FILE *vectors = fopen("tests/solutions.txt", "r");
        require(vectors != NULL, "open vectors failed");
        char line[256]; unsigned count = 0;
        while (fgets(line, sizeof line, vectors)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char *separator = strchr(line, '|');
            require(separator != NULL && separator - line == 14, "invalid vector");
            *separator++ = 0;
            unsigned rank;
            require(cube_parse_rank(line, &rank), "invalid vector state");
            unsigned length = 0;
            for (char *move = strtok(separator, " \r\n"); move;
                 move = strtok(NULL, " \r\n")) ++length;
            require(length == exact[rank], "vector length disagrees with oracle");
            run_case("RV32_ISS", line, (int) length, "supplied-vector"); ++count;
        }
        require(!ferror(vectors) && count == 8, "incomplete vectors");
        fclose(vectors); tables_free();
    } else three_cases("RV32_ISS");
    static const char *const invalid[] = {
        "", "123", "11345671111111", "12345672111111",
        "82345671111111", "12345674111111"
    };
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        run_case("RV32_ISS", invalid[i], -2, "invalid");
    three_cases("RV32_5S");
    require(fclose(results) == 0, "close results failed");
    path_for(path, "summary.txt");
    FILE *summary = fopen(path, "w"); require(summary != NULL, "open summary failed");
    fprintf(summary, "All %u checks passed; hard inputs=%u\n", completed, hard_count);
    if (hard_count) fprintf(summary,
        "Hard retired instructions: min=%" PRIu64 ", mean=%.2f, max=%" PRIu64
        " (%s)\n", minimum, (double) total / hard_count, maximum, worst);
    require(fclose(summary) == 0, "close summary failed");
    printf("All %u checks passed; hard inputs=%u\n", completed, hard_count);
    if (hard_count) printf("Hard maximum: %" PRIu64 " (%s)\n", maximum, worst);
    printf("Results: %s/results.csv\n", directory);
    free(image);
    return 0;
}
