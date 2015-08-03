### Duplicut ###

A very fast tool for removing duplicate lines from a file
without sorting it.

[![Build Status](https://secure.travis-ci.org/nil0x42/duplicut.png?branch=master)](http://travis-ci.org/nil0x42/duplicut)


#### Quick start:
```sh
make release
./duplicut <WORDLIST> -o <OUTPUT_FILE>
```

---------------------------------------------------------------------
#### Overview ####

While building statictically optimized wordlists for password cracking
purposes, i have needed to remove all duplicates quickly, even if the
wordlist 

While creating wordlists of passwords from different sources, i frequently
need to remove duplicates quickly, even on huge wordlists, without changing
the order.

That is how duplicut was born.

```
Usage: duplicut [OPTION]... [INFILE] -o [OUTFILE]
Remove duplicate lines from INFILE without sorting.

Options:
-o, --outfile <FILE>       Write result to <FILE>
-t, --threads <NUM>        Max threads to use (default max)
-m, --memlimit <VALUE>     Limit max used memory (default max)
-l, --line-max-size <NUM>  Max line size (default 14)
-p, --printable            Filter ascii printable lines
-h, --help                 Display this help and exit
-v, --version              Output version information and exit

Example: duplicut wordlist.txt -o new-wordlist.txt
```

* **Features**:
    - Handle huge wordlists, even those whose size exceeds available RAM.
    - Line max length based filtering (-l option).
    - Ascii printable chars based filtering (-p option).
    - Press any key to get program status.

* **Implementation**:
    - Written in pure C code, designed to be fast.
    - Compressed hash map items on 64 bit platforms.
    - **[TODO]:** Multi threaded application.
    - **[TODO]:** Uses huge memory pages to increase performance.

* **Limitations**:
    - Any line longer than 255 chars is ignored.
    - Heavily tested on Linux x64, mostly untested on other platforms.
