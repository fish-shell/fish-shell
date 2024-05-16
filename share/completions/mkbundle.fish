function __fish_mkbundle_cross_args
    mkbundle --list-targets | awk -F '	- ' '/^Targets available/ { exit } !/^Available targets/ { printf "%s\t%s\n", gensub(/^\t/, "", 1, $1), $2 }'
end

# Options
complete -c mkbundle -l config -r -d 'Bundle DLLMAP Mono config file'
complete -c mkbundle -l config-dir -r -d 'Use MONO_CFG_DIR environment variable as config dir'
complete -c mkbundle -n 'not __fish_seen_argument -l sdk -l cross' -l cross -x -a '(__fish_mkbundle_cross_args)' \
    -d 'Create bundle for the specified target platform'
complete -c mkbundle -l deps \
    -d 'Bundle all of the referenced assemblies for the assemblies listed on the command line option'
complete -c mkbundle -l env -x -d 'Specify value for the environment variable'
complete -c mkbundle -l fetch-target -x \
    -d 'Download a precompiled runtime for the specified target from the Mono distribution site'
complete -c mkbundle -l i18n -x -d 'Use encoding tables to ship with the executable'
complete -c mkbundle -s L -r -d 'Use path for look for assemblies'
complete -c mkbundle -l library -r -d 'Embed the dynamic library file'
complete -c mkbundle -l lists-targets -d 'Show all of the available remote cross compilation targets'
complete -c mkbundle -l local-targets -d 'Show all of the available local cross compilation targets'
complete -c mkbundle -l cil-strip -r -d 'Use a CIL stripper that mkbundle will use if able to'
complete -c mkbundle -l in-tree -r \
    -d 'Use a mono source repository to pull the necessary headers for compilation'
complete -c mkbundle -l managed-linker -r \
    -d 'Use mkbundle access to a managed linker to preprocess the assemblies'
complete -c mkbundle -l machine-config -r \
    -d 'Use the given FILE as the machine.config file for the generated application'
complete -c mkbundle -l no-config \
    -d 'Prevent mkbundle from automatically bundling a config file'
complete -c mkbundle -l nodeps \
    -d 'Exclude all the assemblies but those were specified on the command line'
complete -c mkbundle -s o -r -d 'Use output file name'
complete -c mkbundle -l options -r -d 'Specify configuration options to the Mono runtime'
complete -c mkbundle -n 'not __fish_seen_argument -l sdk -l cross' -l sdk -r \
    -d 'Use a path from which mkbundle will resolve the Mono SDK from'
complete -c mkbundle -l target-server -r -d 'Use a different server to provide cross-compiled runtimes'
complete -c mkbundle -l mono-api-struct-path -r \
    -d 'Use file with the definition of the BundleMonoAPI structure'

# Old embedding options
complete -c mkbundle -s c -d 'Produce the stub file, do not compile the resulting stub'
complete -c mkbundle -o oo -r \
    -d 'Specify the name to be used for the helper object file that contains the bundle'
complete -c mkbundle -l keeptemp \
    -d 'Prevent mkbundle from deleting temporary files that it uses to produce the bundle'
complete -c mkbundle -l nomain -d 'Generate the host stub without a main() function'
complete -c mkbundle -l static -d 'Statically link to mono and glib'
complete -c mkbundle -s z -d 'Compress the assemblies before embedding'

# AOT options
complete -c mkbundle -l aot-runtime -r \
    -d 'Use the path to the mono runtime to use for AOTing assemblies'
complete -c mkbundle -l aot-dedup \
    -d 'Deduplicate AOT\'ed methods based on a unique mangling of method names'
complete -c mkbundle -l aot-mode -x \
    -a 'full\t"Generate the necessary stubs to not require runtime code generation" llvmonly\t"Do the same, but force all codegen to go through the llvm backend"' \
    -d 'Specifiy AOT mode'
