#!/bin/bash

./x264 --profile baseline   \
       --input-res  352x240 \
       --slices 1           \
       --no-deblock         \
       --no-scenecut        \
       --no-mbtree          \
       --constrained-intra  \
       --frames 50          \
       --no-interlaced      \
       --rc4-keyfile ../angexp/key.bin  \
       --ref 1              \
       -o ../videos/coded/good.mpg.264        \
          ../videos/raw/good.mpg

