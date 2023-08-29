complete -x -c asciidoctor -k -a "(__fish_complete_suffix .asciidoc .adoc .ad .asc .txt)"

# Security Settings
complete -c asciidoctor -s B -l base-dir -d "Base directory containing the document"
complete -x -c asciidoctor -s S -l safe-mode -a "unsafe safe server secure" -d "Set safe mode level"
complete -c asciidoctor -l safe -d "Set safe mode level to safe"

# Document Settings
complete -c asciidoctor -s a -l attribute -d "Define a document attribute"
complete -x -c asciidoctor -s b -l backend -a "html5 docbook5 manpage" -d "Backend output file format"
complete -x -c asciidoctor -s d -l doctype -a "article book manpage inline" -d "Document type"

# Document Conversion
complete -c asciidoctor -s D -l destination-dir -d "Destination output directory"
complete -c asciidoctor -s E -l template-engine -d "Template engine to use"
complete -x -c asciidoctor -l eruby -a "erb erubis" -d "eRuby implementation to use"
complete -c asciidoctor -s I -l load-path -d "Add a directory to the load path"
complete -c asciidoctor -s n -l section-numbers -d "Auto-number section titles"
complete -c asciidoctor -s o -l out-file -d "Output file"
complete -c asciidoctor -s R -l source-dir -d "Source directory"
complete -c asciidoctor -s r -l require -d "Require the specified library"
complete -c asciidoctor -s s -l no-header-footer -d "Output an embedded document"
complete -c asciidoctor -s T -l template-dir -d "A directory containing custom converter templates"

# Processing Information
complete -x -c asciidoctor -l failure-level -a "WARN ERROR INFO" -d "Minimum logging level"
complete -c asciidoctor -s q -l quiet -d "Be quiet"
complete -c asciidoctor -l trace -d "Include backtrace information"
complete -c asciidoctor -s v -l verbose -d "Be verbose"
complete -c asciidoctor -s w -l warnings -d "Turn on script warnings"
complete -c asciidoctor -s t -l timings -d "Print timings report"

# Program Information
complete -x -c asciidoctor -s h -l help -a manpage -d "Print a help message"
complete -c asciidoctor -s V -l version -d "Print program version"
