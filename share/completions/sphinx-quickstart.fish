complete -c sphinx-quickstart -s q -l quiet -d "Quiet mode"
complete -c sphinx-quickstart -s h -l help -d "Display usage summary"
complete -c sphinx-quickstart -l version -d "Display Sphinx version"

# Structure Options
complete -c sphinx-quickstart -l sep -d "Separate source and build directories"
complete -c sphinx-quickstart -l dot -d "Replacement for dot in _templates etc"

# Project Basic Options
complete -c sphinx-quickstart -s p -l project -d "Project name"
complete -c sphinx-quickstart -s a -l author -d "Author names"
complete -c sphinx-quickstart -s v -d "Version of project"
complete -c sphinx-quickstart -s r -l release -d "Release of project"
complete -c sphinx-quickstart -s l -l language -d "Document language"
complete -c sphinx-quickstart -l suffix -d "Source file suffix"
complete -c sphinx-quickstart -l master -d "Master document name"

# Extension Options
complete -c sphinx-quickstart -l ext-autodoc -d "Enable autodoc extension"
complete -c sphinx-quickstart -l ext-doctest -d "Enable doctest extension"
complete -c sphinx-quickstart -l ext-intersphinx -d "Enable intersphinx extension"
complete -c sphinx-quickstart -l ext-todo -d "Enable todo extension"
complete -c sphinx-quickstart -l ext-coverage -d "Enable coverage extension"
complete -c sphinx-quickstart -l ext-imgmath -d "Enable imgmath extension"
complete -c sphinx-quickstart -l ext-mathjax -d "Enable mathjax extension"
complete -c sphinx-quickstart -l ext-ifconfig -d "Enable ifconfig extension"
complete -c sphinx-quickstart -l ext-viewcode -d "Enable viewcode extension"
complete -c sphinx-quickstart -l ext-githubpages -d "Enable githubpages extension"
complete -c sphinx-quickstart -l extensions -d "Enable arbitrary extensions"

# Makefile and Batchfile Creation Options
complete -c sphinx-quickstart -s m -l use-make-mode -d "Use make-mode"
complete -c sphinx-quickstart -s M -l no-use-make-mode -d "Not use make-mode"
complete -c sphinx-quickstart -l makefile -d "Create makefile"
complete -c sphinx-quickstart -l no-makefile -d "Not create makefile"
complete -c sphinx-quickstart -l batchfile -d "Create batchfile"
complete -c sphinx-quickstart -l no-batchfile -d "Not create batchfile"

# Project templating
complete -c sphinx-quickstart -s t -l templatedir -d "Template directory for template files"
complete -c sphinx-quickstart -s d -d "Define a template variable"
