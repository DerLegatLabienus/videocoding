#!/bin/bash
cgdb --args                 \
./x264 --profile baseline   \
       --input-res  352x240 \
       --slices 1           \
       --no-deblock         \
       --no-scenecut        \
       --no-mbtree          \
       --constrained-intra  \
       --frames 50          \
       --no-interlaced      \
       --rc4-keyfile ../angexp/key_dec.bin \
       --ref 1              \
       -o Aladin.264        \
          Aladin.mpg

