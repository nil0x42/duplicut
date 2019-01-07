#pragma once

#define PROGNAME                "duplicut"
#define PROJECT_VERSION         "1.0"
#define PROJECT_URL             "http://github.com/nil0x42/duplicut"

/* max hashmap size (factor based on available memory) */
#define HMAP_MAX_SIZE           (0.2)

/* hashmap maximum load factor */
#define HMAP_LOAD_FACTOR        (0.5)

/* medium line size for determining hashmap min size */
#define MEDIUM_LINE_BYTES       (8)

/* char used to mark duplicate lines for removal */
#define DISABLED_LINE           '\0'

/* enable/disable multithreading feature */
#define MULTITHREADING          (0)

/* minimum needed memory (change it with care... can throw bugs if too low */
#define MIN_MEMORY              (65536)

/* program status is displayed periodically instead of waiting keypress */
#define DEBUG_PROGRAM_STATUS    (0)
