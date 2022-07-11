complete choose --no-files

# flags:
complete choose -s c -l character-wise -d "Choose fields by character number"
complete choose -s d -l debug -d "Activate debug mode"
complete choose -s x -l exclusive -d "Use exclusive ranges, similar to array indexing in many programming languages"
complete choose -x -s h -l help -d "Prints help information"
complete choose -s n -l non-greedy -d "Use non-greedy field separators"
complete choose -l one-indexed -d "Index from 1 instead of 0"
complete choose -s V -l version -d "Prints version information"

# options:
complete choose -x -s f -l field-separator -d "Specify field separator other than whitespace, using Rust `regex` syntax"
complete choose -r -s i -l input -d "Specify input file"
complete choose -x -s o -l output-field-separator -d "Specify output field separator"
