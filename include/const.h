#pragma once

#define PROGNAME                "duplicut"
#define PROJECT_VERSION         "2.2"
#define PROJECT_URL             "http://github.com/nil0x42/duplicut"

/* portion of available memory to allow to hashmap */
#define HMAP_MAX_SIZE           (0.5)

/* ideal portion of hashmap to be filled */
#define HMAP_LOAD_FACTOR        (0.5)

/* medium line size for determining hashmap min size */
#define MEDIUM_LINE_BYTES       (8)

/* char used to mark duplicate lines for removal */
#define DISABLED_LINE           '\0'

/* enable/disable multithreading feature */
#define MULTITHREADING          (1)

/* minimum needed memory (change with care... can throw bugs if too low */
#define MIN_MEMORY              (65536)

/* display program status periodically instead of waiting keypress */
#define DEBUG_PROGRAM_STATUS    (0)
