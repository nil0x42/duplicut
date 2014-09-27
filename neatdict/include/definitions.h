#ifndef DEFINITIONS_H
# define DEFINITIONS_H

# define PROGNAME           "neatdict"
# define PROJECT_VERSION    "0.1a"
# define PROJECT_URL        "http://github.com/nil0x42/neatcrack"

# define HMAP_LOAD_FACTOR   (0.5)        // fill at most 50% of the hmap
# define MEDIUM_LINE_BYTES  (5)          // a line takes at least ~= 5 bytes
# define KEEP_FREE_MEMORY   (0xc800000L) // 200MB RAM kept unused as security

# define CHUNK_FILENAME     "/neatdict-XXXXXX.chunk"
# define CHUNK_PATHSIZE     (256)

# define DISABLED_LINE      '\0'

#endif /* DEFINITIONS_H */
