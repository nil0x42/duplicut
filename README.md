### Duplicut ###

Find & **remove duplicate** entries from a wordlist, **without changing the order** and **without getting
OOM** even on huge wordlists

[![Build Status](https://secure.travis-ci.org/nil0x42/duplicut.png?branch=master)](http://travis-ci.org/nil0x42/duplicut)
[![Mentioned in Awesome Penetration Testing](https://awesome.re/mentioned-badge.svg)](https://github.com/enaqx/awesome-pentest)
[![twitter follow](https://img.shields.io/twitter/follow/nil0x42?label=Follow%20nil0x42%20%21&style=social)](https://twitter.com/intent/follow?screen_name=nil0x42)

#### Quick start:
```sh
make release
./duplicut <WORDLIST_WITH_DUPLICATES> -o <NEW_CLEAN_WORDLIST>
```

---------------------------------------------------------------------
#### Overview ####

Building statictically optimized wordlists for password
cracking often requires to be able to find and remove
duplicate entries without changing the order.

Unfortunately, existing *duplicate removal tools* are not
able to handle **very huge wordlists** without crashing
due to insufficient memory:

![][img-1-comparison]



Duplicut is written in C, and optimized to be as
_**fast**_ and _**memory frugal**_ as possible.

For example, duplicut hashmap saves up to 50% space by packing
`size` information within line pointer's [extra bits][tagged-pointer]:

![][img-2-line-struct]



If the whole file doesn't fit in memory, file is split into 
![][latex-n] chunks, and each one is tested against next chunks.

So complexity is equal to ![][latex-n]th *triangle number*:
![][img-3-chunked-processing]


---------------------------------------------------------------------
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


[img-1-comparison]: data/img/1-comparison.png
[img-2-line-struct]: data/img/2-line-struct.png
[img-3-chunked-processing]: data/img/3-chunked-processing.png

[tagged-pointer]: https://en.wikipedia.org/wiki/Tagged_pointer

[latex-n]: http://www.sciweavers.org/tex2img.php?fs=15&eq=n
[latex-nth-triangle]: http://www.sciweavers.org/tex2img.php?fs=32&eq=%5Csum_%7Bk%3D1%7D%5Enk
