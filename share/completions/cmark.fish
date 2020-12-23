complete -k -x -c cmark -a "
(
    __fish_complete_suffix .md
    __fish_complete_suffix .markdown
)
"

complete -x -c cmark -s t -l to -a "html man xml latex commonmark" -d "Output format"
complete -c cmark -l width -d "Wrap width"
complete -c cmark -l hardbreaks -d "Treat newlines as hard line breaks"
complete -c cmark -l nobreaks -d "Render soft line breaks as spaces"
complete -c cmark -l sourcepos -d "Include source position attribute"
complete -c cmark -l validate-utf8 -d "Validate UTF-8"
complete -c cmark -l smart -d "Use smart punctuation"
complete -c cmark -l unsafe -d "Render raw HTML and dangerous URLs"
complete -c cmark -l help -d "Print usage information"
complete -c cmark -l version -d "Print version"
