# Completions for Docutils common options
__fish_complete_docutils rst2odt

# Completions for Docutils standalone reader options
__fish_complete_docutils_standalone_reader rst2odt

# Completions for Docutils ODF options
complete -c rst2odt -l stylesheet -d "Specify a stylesheet"
complete -c rst2odt -l odf-config-file -d "Specify additional ODF options"
complete -c rst2odt -l cloak-email-addresses -d "Enable obfuscate email addresses"
complete -c rst2odt -l no-cloak-email-addresses -d "Disable obfuscate email addresses"
complete -c rst2odt -l table-border-thickness -d "Specify the thickness of table borders"
complete -c rst2odt -l add-syntax-highlighting -d "Enable syntax highlighting"
complete -c rst2odt -l no-syntax-highlighting -d "Disable syntax highlighting"
complete -c rst2odt -l create-sections -d "Create sections"
complete -c rst2odt -l no-sections -d "Don't create sections"
complete -c rst2odt -l create-links -d "Create links"
complete -c rst2odt -l no-links -d "Don't create links"
complete -c rst2odt -l endnotes-end-doc -d "Generate endnotes"
complete -c rst2odt -l no-endnotes-end-doc -d "Generate footnotes"
complete -c rst2odt -l generate-list-toc -d "Generate a bullet list TOC"
complete -c rst2odt -l generate-oowriter-toc -d "Generate an ODF TOC"
complete -c rst2odt -l custom-odt-header -d "Specify a custom header"
complete -c rst2odt -l custom-odt-footer -d "Specify a custom footer"
