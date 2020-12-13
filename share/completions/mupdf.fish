complete -c mupdf -k -x -a "(__fish_complete_suffix .pdf)"

complete -c mupdf -s p -d Description
complete -c mupdf -s r -d Resolution
complete -c mupdf -x -s A -a "(seq 0 8)" -d "Set anti-aliasing quality in bits"
complete -c mupdf -x -s C -d "Tint color in RRGGBB"
complete -c mupdf -s I -d "Invert colors"
complete -c mupdf -x -s W -d "Page width for EPUB layout"
complete -c mupdf -x -s H -d "Page height for EPUB layout"
complete -c mupdf -x -s S -d "Font size for EPUB layout"
complete -c mupdf -r -s U -d "User style sheet for EPUB layout"
complete -c mupdf -s X -d "Disable document styles for EPUB layout"
