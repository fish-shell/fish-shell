# Completions for Docutils common options
__fish_complete_docutils rst2xml

# Completions for Docutils standalone reader options
__fish_complete_docutils_standalone_reader rst2xml

# Completions for Docutils XML Writer options
complete -c rst2xml -l newlines -d "Generate XML with newlines"
complete -c rst2xml -l indents -d "Generate XML with indents"
complete -c rst2xml -l no-xml-declaration -d "Omit the XML declaration"
complete -c rst2xml -l no-doctype -d "Omit the DOCTYPE"
