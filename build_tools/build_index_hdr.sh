#!/bin/sh

TOC_TXT=$1
env awk "{if (\$0 ~ /@toc@/){ system(\"cat ${TOC_TXT}\");} else{ print \$0;}}"

