# Completions for the Visual C# Interactive Compiler(Roslyn)

complete -c csi -o help -o "?" -d "Display this usage message"
complete -c csi -o version -d "Display the version and exit"
complete -c csi -s i -d "Drop to REPL after executing the specified script"
complete -c csi -o "r:" -o "reference:" -d "Reference metadata from the specified assembly file(s)"
complete -c csi -o "lib:" -o "libPath:" -o "libPaths:" -d "List of directories where to look for libraries specified by #r directive"
complete -c csi -o "u:" -o "using:" -o "usings:" -o "import:" -o "imports:" -d "Define global namespace using"
