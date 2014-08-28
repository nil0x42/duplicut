#include <string.h>
#include "definitions.h"
#include "error.h"



/* # define MAP(fd, off, len) mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, off) */
/* void            map_chunk(t_chunk *chunk) */
/* { */
/*     chunk->ptr = MAP(chunk->file.fd, chunk->map.offset, chunk->map.size); */
/*     if (chunk->ptr == MAP_FAILED) */
/*         die("%s: could not map chunk %d", chunk->file.name, chunk->id); */
/* } */
