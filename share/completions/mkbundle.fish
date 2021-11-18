function __get_mkbundle_cross_args
 mkbundle --list-targets | awk -F '	- ' '/^Targets available/ { exit } !/^Available targets/ { printf "%s\t%s\n", gensub(/^\t/, "", 1, $1), $2 }'
end

# Options
complete --command mkbundle --long-option help --description 'Show help and exit'
complete --command mkbundle --long-option version --description 'Show version and exit'

complete --command mkbundle --long-option config --require-parameter --description 'Specify that a DLLMAP Mono config file must be bundled as well'
complete --command mkbundle --long-option config-dir --require-parameter --description 'When passed, DIR will be set for the MONO_CFG_DIR environment variable'
complete --command mkbundle --long-option cross --no-files --require-parameter --arguments '(__get_mkbundle_cross_args)' --description 'When passed, DIR will be set for the MONO_CFG_DIR environment variable'
complete --command mkbundle --long-option deps --description 'Bundle all of the referenced assemblies for the assemblies listed on the command line option'
complete --command mkbundle --long-option env --require-parameter --description 'Hardcode an environment variable at runtime for KEY to be mapped to VALUE'
complete --command mkbundle --long-option fetch-target --no-files --require-parameter --description 'Download a precompiled runtime for the specified target from the Mono distribution site'
complete --command mkbundle --long-option i18n --no-files --require-parameter --description 'Specify which encoding tables to ship with the executable'
complete --command mkbundle --short-option L --require-parameter --description 'Add the \'path\' do the search list for assemblies'
complete --command mkbundle --long-option library --require-parameter --description 'Embed the dynamic library file pointed to by \'PATH\' and optionally give it the name \'LIB\' into the bundled executable'
complete --command mkbundle --long-option lists-targets --description 'List all of the available local cross compilation targets available as precompiled binaries on the Mono distribution server'
complete --command mkbundle --long-option cil-strip --require-parameter --description 'Provide a CIL stripper that mkbundle will use if able to'
complete --command mkbundle --long-option in-tree --description 'Provide mkbundle with a mono source repository from which to pull the necessary headers for compilation'
complete --command mkbundle --long-option managed-linker --description 'Provide mkbundle access to a managed linker to preprocess the assemblies'
complete --command mkbundle --long-option machine-config --description 'Use the given FILE as the machine.config file for the generated application'
complete --command mkbundle --long-option no-config --description 'Prevent mkbundle from automatically bundling a config file'
complete --command mkbundle --long-option nodeps --description 'Only include the assemblies that were specified on the command line'
complete --command mkbundle --short-option o --require-parameter --description 'Places the output on \'out\''
complete --command mkbundle --long-option options --require-parameter --description 'Pass configuration options to the Mono runtime'
complete --command mkbundle --long-option sdk --require-parameter --description 'specify a path from which mkbundle will resolve the Mono SDK from'
complete --command mkbundle --long-option target-server --require-parameter --description 'Specify a different server to provide cross-compiled runtimes'
complete --command mkbundle --long-option mono-api-struct-path --require-parameter --description ''

# Old embedding options
complete --command mkbundle --short-option c --description 'Produce the stub file, do not compile the resulting stub'
complete --command mkbundle --old-option oo --description 'Specifies the name to be used for the helper object file that contains the bundle'
complete --command mkbundle --long-option keeptemp --description 'Prevent mkbundle from deleting temporary files that it uses to produce the bundle'
complete --command mkbundle --long-option nomain --description 'Generate the host stub without a main() function'
complete --command mkbundle --long-option static --description 'Cause mkbundle statically link to mono and glib'
complete --command mkbundle --short-option z --description 'Compress the assemblies before embedding'

# AOT options
complete --command mkbundle --long-option aot-runtime --description 'Provide the path to the mono runtime to use for AOTing assemblies'
complete --command mkbundle --long-option aot-dedup --description 'Deduplicate AOT\'ed methods based on a unique mangling of method names'
complete --command mkbundle --long-option aot-mode --no-files --require-parameter --arguments 'full\t"Generate the necessary stubs to not require runtime code generation" llvmonly\t"Do the same, but force all codegen to go through the llvm backend"' --description ''
