#pragma once

#include "file.h"


typedef struct  chunk
{
    char        *ptr;
    char        *endptr;
}               t_chunk;


/* source file: tag_duplicates.c */
void            tag_duplicates(t_file *file);
