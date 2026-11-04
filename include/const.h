#pragma once

#define PROGNAME                "duplicut"
#define PROJECT_VERSION         "v2.5"
#define PROJECT_URL             "http://github.com/nil0x42/duplicut"

/* cast a preprocessor int to string */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* portion of the memory budget allocated to the hash table */
#define HMAP_MAX_SIZE           (0.5)

/* target hash table load factor for sizing */
#define HMAP_LOAD_FACTOR        (0.5)

/* maximum hash table load before continuing in another pass */
#define HMAP_MAX_LOAD           (0.7)

/* fallback line size when no accepted sample is available */
#define AVG_LINE_BYTES          (8)

/* char used to mark duplicate lines for removal */
#define DISABLED_LINE           '\0'

/* enable/disable multithreading feature */
#define MULTITHREADING          (1)

/* minimum needed memory (change with care... can throw bugs if too low */
#define MIN_MEMORY              (65536)

/* max supported max-line-size */
#define MAX_MAX_LINE_SIZE       4095
