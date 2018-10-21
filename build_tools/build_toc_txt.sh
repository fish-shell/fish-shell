#!/bin/sh

# Usage: build_toc_txt.sh $(HDR_FILES:index.hdr=index.hdr.in) > toc.txt

# Ugly hack to set the toc initial title for the main page
echo "- <a href=\"index.html\" id=\"toc-index\">fish shell documentation - ${FISH_BUILD_VERSION}</a>" > toc.txt
# The first sed command captures the page name, followed by the description
# The second sed command captures the command name \1 and the description \2, but only up to a dash
# This is to reduce the size of the TOC in the command listing on the main page
for i in $@; do
	NAME=`basename $i .hdr`
	NAME=`basename $NAME .hdr.in`
  env sed <$i >>toc.txt -n \
    -e 's,.*\\page *\([^ ]*\) *\(.*\)$,- <a href="'$NAME'.html" id="toc-'$NAME'">\2</a>,p' \
    -e 's,.*\\section *\([^ ]*\) *\(.*\) - .*$,  - <a href="'$NAME'.html#\1">\2</a>,p' \
    -e 's,.*\\section *\([^ ]*\) *\(.*\)$,  - <a href="'$NAME'.html#\1">\2</a>,p'
done
