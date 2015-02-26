#pragma once

#define PROGNAME            "duplicut"
#define PROJECT_VERSION     "0.1a"
#define PROJECT_URL         "http://github.com/nil0x42/duplicut"

/* max hashmap size (factor based on availabled memory) */
#define HMAP_MAX_SIZE       (0.2)

/* hashmap maximum load factor */
#define HMAP_LOAD_FACTOR    (0.5)

/* medium line size for determining hashmap min size */
#define MEDIUM_LINE_BYTES   (6)

/* this char is written at start of lines as a `removal mark` */
#define DISABLED_LINE       '\0'
