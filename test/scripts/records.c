#include <assert.h>
#include <sys/wait.h>
#include "../../src/partition.c"


static void     check_record(uint64_t id, uint16_t size)
{
    unsigned char expected[RECORD_HEADER_MAX] = {0};
    unsigned char header[RECORD_HEADER_MAX];
    unsigned char input[RECORD_HEADER_MAX] = {0};
    uint64_t result;
    uint16_t length;
    size_t   width = record_header_size(id == UINT64_MAX ? id : id + 1);
    unsigned bit;

    for (bit = 0; bit < 76; ++bit)
        if ((bit < 12 ? (uint64_t)size >> bit : id >> (bit - 12)) & 1)
            expected[bit / 8] |= 1U << (bit % 8);
    encode_record(header, id, size);
    assert(memcmp(header, expected, sizeof(header)) == 0);
    memcpy(input, expected, width);
    assert(decode_record(input, &result, &length));
    assert(result == id && length == size);
    input[9] |= 0x80;
    assert(!decode_record(input, &result, &length));
}


static void     test_widths(void)
{
    uint64_t boundary;
    uint64_t random = 22;
    unsigned bits;
    unsigned i;
    size_t   width = 5;

    assert(record_header_size(0) == 5);
    assert(record_header_size(1) == 5);
    assert(record_header_size(UINT64_MAX) == 10);
    check_record(0, 1);
    check_record(UINT64_MAX, MAX_MAX_LINE_SIZE);
    for (bits = 28; bits <= 60; bits += 8, ++width)
    {
        boundary = UINT64_C(1) << bits;
        assert(record_header_size(boundary - 1) == width);
        assert(record_header_size(boundary) == width);
        assert(record_header_size(boundary + 1) == width + 1);
        check_record(boundary - 1, 1);
        check_record(boundary, MAX_MAX_LINE_SIZE);
        check_record(boundary + 1, 17);
    }
    for (i = 0; i < 2000; ++i)
    {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        check_record(random, i % MAX_MAX_LINE_SIZE + 1);
    }
    puts("[+] header widths, boundary indices and independent byte vectors");
}


static void     test_sizes(void)
{
    struct partition parts[2];
    struct partition_work work;
    uint64_t lines;
    size_t   width;

    for (width = 5; width <= 9; ++width)
    {
        lines = width == 5 ? 5 : (UINT64_C(1) << (8 * (width - 1) - 12)) + 1;
        parts[0] = (struct partition){.end = lines - 2, .lines = lines - 2};
        parts[1] = (struct partition){.end = 8, .lines = 2};
        work = (struct partition_work){.parts = parts, .count = 2, .lines = lines};
        assert(size_partitions(&work, lines + 6) == lines + 6 + lines * width);
        assert(work.header_size == width);
        assert(parts[0].end == (lines - 2) * (width + 1));
        assert(parts[1].end == 8 + 2 * width);
    }
    puts("[+] exact range sizes after global header selection");
}


static void     test_buffer(size_t width)
{
    unsigned char header[RECORD_HEADER_MAX];
    unsigned char result[RECORD_HEADER_MAX] = {0};
    char padding[BUF_SIZE - 2] = {0};
    char buffer[BUF_SIZE];
    char line[MAX_MAX_LINE_SIZE];
    char actual[MAX_MAX_LINE_SIZE];
    FILE *file = tmpfile();
    struct partition part = {.buffer = buffer};
    struct partition_input input;

    assert(file != NULL);
    memset(line, 'x', sizeof(line));
    encode_record(header, 3, sizeof(line));
    append_partition(fileno(file), &part, padding, sizeof(padding));
    append_partition(fileno(file), &part, header, width);
    append_partition(fileno(file), &part, line, sizeof(line));
    write_partition(fileno(file), &part);
    input = (struct partition_input){.fd = fileno(file), .end = part.cursor};
    read_partition(&input, padding, sizeof(padding));
    read_partition(&input, result, width);
    read_partition(&input, actual, sizeof(actual));
    assert(memcmp(header, result, sizeof(header)) == 0);
    assert(memcmp(line, actual, sizeof(line)) == 0);
    fclose(file);
}


static void     test_overflow(size_t width, const char *invalid)
{
    const char *words[] = {"alpha", "beta", "alpha", "gamma", "beta"};
    unsigned char header[RECORD_HEADER_MAX];
    unsigned char duplicates = 0;
    char buffer[BUF_SIZE];
    char area[16];
    t_line table[4];
    FILE *file = tmpfile();
    struct partition part = {.buffer = buffer};
    struct partition_work work = {.lines = 5, .header_size = width,
        .duplicates = &duplicates};
    uint64_t id;
    size_t length;

    assert(file != NULL);
    work.fd = fileno(file);
    for (id = 0; id < 5; ++id)
    {
        length = strlen(words[id]);
        encode_record(header, invalid && !strcmp(invalid, "index") ? 5 : id,
            invalid && !strcmp(invalid, "length") ? 0 : length);
        if (invalid && !strcmp(invalid, "padding"))
            header[9] |= 0x80;
        append_partition(work.fd, &part, header,
            invalid && !strcmp(invalid, "header") ? width - 1 : width);
        if (invalid && !strcmp(invalid, "header"))
            break;
        append_partition(work.fd, &part, words[id], length);
    }
    write_partition(work.fd, &part);
    part.end = part.cursor;
    if (invalid && !strcmp(invalid, "payload"))
        --part.end;
    clean_partition(&work, &part, table, 4, area, sizeof(area));
    assert(duplicates == ((1U << 2) | (1U << 4)));
    fclose(file);
}


static void     test_invalid(const char *invalid)
{
    int   status;
    pid_t child;

    assert(fflush(stdout) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0)
    {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        if (!strcmp(invalid, "size"))
        {
            struct partition_work work = {.lines = UINT64_MAX};

            size_partitions(&work, 0);
        }
        else
            test_overflow(10, invalid);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) != 0);
}


int             main(void)
{
    size_t width;

    test_widths();
    test_sizes();
    for (width = 5; width <= RECORD_HEADER_MAX; ++width)
    {
        test_buffer(width);
        test_overflow(width, NULL);
    }
    puts("[+] all widths across buffer boundaries and bounded overflow passes");
    test_invalid("size");
    test_invalid("index");
    test_invalid("length");
    test_invalid("padding");
    test_invalid("header");
    test_invalid("payload");
    puts("[+] oversized ranges, invalid fields and truncated records");
    return (0);
}
