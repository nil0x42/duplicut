### Duplicut ###

A very fast tool for removing duplicate lines from a file
without sorting it.

[![Build Status](https://secure.travis-ci.org/nil0x42/duplicut.png?branch=master)](http://travis-ci.org/nil0x42/duplicut)

---------------------------------------------------------------------
#### Overview ####

This tool has been created by a guy sad of not being able to remove
duplicate lines on huge wordlists whose order was statistically
optimised for password cracking purposes.

Usage:
```sh
./duplicut ~/wordlists/raw-list.txt
```

* **Features**:
    - Handles very huge files whose size exceeds available memory.
    - Duplicate lines can be removed from multiple files.
    - Also handle size based line removal.

* **Implementation**:
    - Written in pure C code to be the fastest possible.
    - Clever memory management based on file chunk mappings.
    - Compressed hash map items on 64 bit platforms.
    - Multi threaded application for fast chunk comparisons.

* **Limitations**:
    - Any line longer than 255 chars is ignored.
    - Only supported on GNU/Linux - 64bits.
