# Define the completions for the qjsc command
# QuickJS Compiler version 2021-03-27
# usage: qjsc [options] [files]

# options are:
# -c          only output bytecode in a C file
# -e          output main() and bytecode in a C file (default = executable output)
# -o output   set the output filename
# -N cname    set the C name of the generated data
# -m          compile as Javascript module (default=autodetect)
# -D module_name         compile a dynamically loaded module or worker
# -M module_name[,cname] add initialization code for an external C module
# -x          byte swapped output
# -p prefix   set the prefix of the generated C names
# -S n        set the maximum stack size to 'n' bytes (default=262144)
# -flto       use link time optimization
# -fbignum    enable bignum extensions
# -fno-[date|eval|string-normalize|regexp|json|proxy|map|typedarray|promise|module-loader|bigint]
#             disable selected language features (smaller code size)
# from https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=quickjs 2021.03.27

complete -c qjsc -s c -d 'Only output bytecode in a C file'
complete -c qjsc -s e -d 'Output main() and bytecode in a C file (default = executable output)'
complete -c qjsc -s o -r -d 'Set the output filename'
complete -c qjsc -s N -r -d 'Set the C name of the generated data'
complete -c qjsc -s m -d 'Compile as Javascript module (default=autodetect)'
complete -c qjsc -s D -r -d 'Compile a dynamically loaded module or worker'
complete -c qjsc -s M -r -d 'Add initialization code for an external C module'
complete -c qjsc -s x -d 'Byte swapped output'
complete -c qjsc -s p -r -d 'Set the prefix of the generated C names'
complete -c qjsc -s S -r -d 'Set the maximum stack size to 'n' bytes (default=262144)'
complete -c qjsc -o flto -d 'Use link time optimization'
complete -c qjsc -o fbignum -d 'Enable bignum extensions'
complete -c qjsc -o fno-date -d 'Disable the date extension'
complete -c qjsc -o fno-eval -d 'Disable the eval extension'
complete -c qjsc -o fno-string-normalize -d 'Disable the string normalize extension'
complete -c qjsc -o fno-regexp -d 'Disable the regexp extension'
complete -c qjsc -o fno-json -d 'Disable the JSON extension'
complete -c qjsc -o fno-proxy -d 'Disable the proxy extension'
complete -c qjsc -o fno-map -d 'Disable the Map extension'
complete -c qjsc -o fno-typedarray -d 'Disable the Typed Array extension'
complete -c qjsc -o fno-promise -d 'Disable the Promise extension'
complete -c qjsc -o fno-module-loader -d 'Disable the module loader extension'
complete -c qjsc -o fno-bigint -d 'Disable the BigInt extension'
