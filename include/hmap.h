#pragma once

#include "line.h"
#include "chunk.h"


struct hmap
{
    t_line      *ptr;
    size_t      size;
};

/* global hash map */
extern struct hmap  g_hmap;

/* source file: hmap.c */
void    init_hmap(size_t size);
void    destroy_hmap(void);
bool    populate_hmap(t_chunk *chunk);
