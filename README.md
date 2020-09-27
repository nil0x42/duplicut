<h1 align="center">Duplicut :scissors:</h1>

<h3 align="center">
    Quickly dedupe massive wordlists, without changing the order
    <a href="https://twitter.com/intent/tweet?text=Duplicut%3A%20Remove%20duplicates%20from%20MASSIVE%20wordlist%2C%20without%20sorting%20it%20(for%20dictionnary-based%20password%20cracking)%20-%20by%20%40nil0x42&url=https://github.com/nil0x42/duplicut">
      <img src="https://img.shields.io/twitter/url?label=tweet&logo=twitter&style=social&url=http%3A%2F%2F0" alt="tweet">
    </a>
</h3>
<br>

<p align="center">
  <a href="https://travis-ci.com/nil0x42/duplicut">
    <img src="https://img.shields.io/travis/com/nil0x42/duplicut?branch=master&logo=travis" alt="travis build">
  </a>
  <a href="https://github.com/enaqx/awesome-pentest">
    <img src="https://awesome.re/mentioned-badge.svg" alt="Mentioned in awesome-pentest">
  </a>
  <a href="https://twitter.com/intent/follow?screen_name=nil0x42" target="_blank">
    <img src="https://img.shields.io/twitter/follow/nil0x42.svg?logo=twitter" akt="follow on twitter">
  </a>
</p>

<div align="center">
  <sub>
    Created by
    <a href="https://twitter.com/nil0x42">nil0x42</a> and
    <a href="https://github.com/nil0x42/duplicut/graphs/contributors">contributors</a>
  </sub>
</div>

<br>

* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

### :book: Overview

Modern password wordlist creation usually implies concatenating
multiple data sources.

Ideally, most probable passwords should stand at start of the wordlist,
so most common passwords are cracked instantly.

With existing *dedupe tools* you are forced to choose
if you prefer to _preserve the order **OR** handle massive wordlists_.

Unfortunately, **wordlist creation requires both**:

![][img-1-comparison]

> **So i wrote duplicut in [highly optimized C] to address this very specific need :nerd_face: :computer:**

* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

### :bulb: Quick start

```sh
git clone https://github.com/nil0x42/duplicut
cd duplicut/ && make
./duplicut wordlist.txt -o clean-wordlist.txt
```


### :wrench: Options

![][img-4-help]

* **Features**:
    - Handle massive wordlists, even those whose size exceeds available RAM
    - Filter lines by max length (`-l` option)
    - Can remove lines containing non-printable ASCII chars (`-p` option)
    - Press any key to show program status at runtime.

* **Implementation**:
    - Written in pure C code, designed to be fast
    - Compressed hashmap items on 64 bit platforms
    - Multithreading support
    - **[TODO]:** Use huge memory pages to increase performance

* **Limitations**:
    - Any line longer than 255 chars is ignored
    - Heavily tested on Linux x64, mostly untested on other platforms.


### :book: Technical Details

#### :small_orange_diamond: 1- Memory optimized:

An `uint64` is enough to index lines in hashmap, by packing
`size` info within pointer's [extra bits][tagged-pointer]:

![][img-2-line-struct]


#### :small_orange_diamond: 2- Massive file handling:

If whole file can't fit in memory, it is split into ![][latex-n]
virtual chunks, then each one is tested against next chunks.

So complexity is equal to ![][latex-n]th *triangle number*:

![][img-3-chunked-processing]


## :bulb: Throubleshotting

If you find a bug, or something doesn't work as expected,
please compile duplicut in debug mode and post an [issue] with
attached output:
```
# debug level can be from 1 to 4
make debug level=1
./duplicut [OPTIONS] 2>&1 | tee /tmp/duplicut-debug.log
```


[highly optimized C]: https://github.com/nil0x42/duplicut/blob/master/src/line.c#L39

[img-1-comparison]: data/img/1-comparison.png
[img-2-line-struct]: data/img/2-line-struct.png
[img-3-chunked-processing]: data/img/3-chunked-processing.png
[img-4-help]: data/img/4-help.png

[issue]: https://github.com/nil0x42/duplicut/issues
[tagged-pointer]: https://en.wikipedia.org/wiki/Tagged_pointer

[latex-n]: http://www.sciweavers.org/tex2img.php?fs=15&eq=n
[latex-nth-triangle]: http://www.sciweavers.org/tex2img.php?fs=32&eq=%5Csum_%7Bk%3D1%7D%5Enk
