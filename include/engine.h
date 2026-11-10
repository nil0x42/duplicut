#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct memstate;
struct line_sample;


enum    e_engine
{
    ENGINE_AUTO,
    ENGINE_MMAP_CHUNKS,
    ENGINE_DISK_PARTITIONS
};


/** Exact input and usable worker capacities after counting partitions. */
struct partition_estimate
{
    uint64_t    lines;
    size_t      contents;
    size_t      bytes;
    size_t      count;
    size_t      workers;
    size_t      entries;
    size_t      capacity;
    size_t      bitmap;
};


/* source file: engine.c */
const char  *engine_name(enum e_engine engine);
void        config_engine(const struct memstate *memory, const struct line_sample *sample);
bool        prefer_partitions(const struct partition_estimate *estimate);
bool        engine_fallback(const char *reason);
