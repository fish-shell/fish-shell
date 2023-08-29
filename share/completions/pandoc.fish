# Pandoc completions written by David Sanson
# Copyright (c) 2018 David Sanson
# Licensed under the GNU General Public License version 2

set -l informats (pandoc --list-input-formats)
set -l outformats (pandoc --list-output-formats)
set -l highlight_styles (pandoc --list-highlight-styles)
set -l datadir $HOME/.pandoc

# Only suggest installed engines
set -l pdfengines
for engine in pdflatex lualatex xelatex latexmk tectonic wkhtmltopdf weasyprint pagedjs-cli prince context pdfroff
    if type -q $engine
        set pdfengines $pdfengines $engine
    end
end

# options that take no arguments
complete -c pandoc -s p -l preserve-tabs
complete -c pandoc -l file-scope
complete -c pandoc -s s -l standalone
complete -c pandoc -l strip-comments
complete -c pandoc -l toc -l table-of-contents
complete -c pandoc -l no-highlight
complete -c pandoc -l self-contained
complete -c pandoc -l html-q-tags
complete -c pandoc -l ascii
complete -c pandoc -l reference-links
complete -c pandoc -l atx-headers
complete -c pandoc -s N -l number-sections
complete -c pandoc -l listings
complete -c pandoc -s i -l incremental
complete -c pandoc -l section-divs
complete -c pandoc -l natbib -l biblatex
complete -c pandoc -l dump-args
complete -c pandoc -l ignore-args
complete -c pandoc -l verbose
complete -c pandoc -l quiet
complete -c pandoc -l fail-if-warnings
complete -c pandoc -l bash-completion
complete -c pandoc -l list-input-formats
complete -c pandoc -l list-output-formats
complete -c pandoc -l list-extensions
complete -c pandoc -l list-highlight-languages
complete -c pandoc -l list-highlight-styles
complete -c pandoc -s v -l version
complete -c pandoc -s h -l help

# options that complete URLs
complete -c pandoc -x -l mathml -l webtex -l mathjax -l katex
complete -c pandoc -x -l katex-stylesheet -s m -l latexmathml
complete -c pandoc -x -l asciimathml -l mimetex -l jsmath -l gladtex
complete -c pandoc -x -s c -l css

# options that use informats and outformats
complete -c pandoc -x -s f -s r -l from -l read -a "$informats"
complete -c pandoc -x -s t -s w -l to -l write -a "$outformats"
complete -c pandoc -x -s D -l print-default-template -a "$outformats"

# TODO: add support for enabling and disabling extensions using the +/- switches

# options that take directories
complete -c pandoc -x -l data-dir -a "(__fish_complete_directories (commandline -ct) "")"
complete -c pandoc -x -l extract-media -a "(__fish_complete_directories (commandline -ct) "")"

# options that take files
complete -c pandoc -r -s o -l output
complete -c pandoc -r -s F -l filter
complete -c pandoc -r -l template
complete -c pandoc -r -l syntax-definition
complete -c pandoc -r -s H -l include-in-header
complete -c pandoc -r -s B -l include-before-body
complete -c pandoc -r -s A -l include-after-body
complete -c pandoc -r -l abbreviations
complete -c pandoc -r -l log
complete -c pandoc -r -l epub-cover-image
complete -c pandoc -r -l epub-metadata
complete -c pandoc -r -l epub-embed-font
complete -c pandoc -r -l citation-abbreviations

# options that take files filtered by extension

complete -c pandoc -r -f -l print-highlight-style -k -a "(__fish_complete_suffix 'theme' )"
complete -c pandoc -r -f -l highlight_style -k -a "(__fish_complete_suffix 'theme' )"
complete -c pandoc -r -f -l csl -k -a "(__fish_complete_suffix 'csl'   )"
complete -c pandoc -r -f -l reference-file -k -a "(__fish_complete_suffix 'odt' 'docx')"
complete -c pandoc -r -f -l bibliography -k -a "(__fish_complete_suffix 'bib' 'bibtex' 'copac' 'json' 'yaml' 'enl' 'xml' 'wos' 'medline' 'mods' 'ria')"
complete -c pandoc -r -f -l lua-filter -k -a "(__fish_complete_suffix 'lua')"

# options that take files in DATADIR
complete -c pandoc -r -s F -l filter -a "(find $datadir/filters -type f 2>/dev/null | string replace -- '$datadir/filters/' '')"
complete -c pandoc -r -l template -a "(find $datadir/templates -type f 2>/dev/null | string replace -- '$datadir/templates/' '')"
complete -c pandoc -r -f -l lua-filter -a "(find $datadir -type f 2>/dev/null | string match -r '.lua\$' | string replace -- '$datadir/' '')"

# options that require arguments which cannot be autocompleted
complete -c pandoc -x -l indented-code-classes
complete -c pandoc -x -s M -l metadata
complete -c pandoc -x -s V -l variable
complete -c pandoc -x -l dpi
complete -c pandoc -x -l columns
complete -c pandoc -r -l resource-path
complete -c pandoc -x -l request-header
complete -c pandoc -x -l number-offset
complete -c pandoc -x -l id-prefix
complete -c pandoc -x -s T -l title-prefix
complete -c pandoc -x -l epub-subdirectory
complete -c pandoc -x -l pdf-engine-opt

# options that take their own arguments

complete -c pandoc -x -l default-image-extension -a 'jpg png pdf svg gif'
complete -c pandoc -x -l base-header-level -a "1 2 3 4 5"
complete -c pandoc -x -l toc-depth -l slide-level -l epub-chapter-level -a "1 2 3 4 5 6"
complete -c pandoc -x -l tab-stop -a "1 2 3 4 5 6 7 8"
complete -c pandoc -x -l track-changes -a "accept reject all"
complete -c pandoc -x -l print-default-data-file -a "reference.odt reference.docx"
complete -c pandoc -x -l print-highlight-style -l highlight-style -a "$highlight_styles"
complete -c pandoc -x -l eol -a "crlf lf native"
complete -c pandoc -x -l wrap -a "auto none preserve"
complete -c pandoc -x -l reference-location -a "block section document"
complete -c pandoc -x -l top-level-division -a "default section chapter part"
complete -c pandoc -x -l email-obfuscation -a "none javascript references"
complete -c pandoc -x -l pdf-engine -a "$pdfengines"
