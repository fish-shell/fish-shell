# Completions for `zig` (https://ziglang.org/)
# Based on version 0.13.0-dev.8+c352845e8

# This function is based on the `__fish_complete_clang` function.
function __fish_complete_zig_cc_and_cpp
    # If the result is for a value, clang only prints the value, so completions
    # for `-std=` print `c++11` and not `-std=c++11` like we need. See #4174.
    set -l prefix (commandline -ct | string replace -fr -- '^(.*=)[^=]*' '$1')

    # first get the completions from clang, with the prefix separated from the value by a comma
    zig cc --autocomplete=(commandline -ct | string unescape | string replace -- "$prefix" "$prefix,") 2>/dev/null |
        # and put it in a format that fish understands
        string replace -r -- '^([^ ]+)\s*(.*)' "$prefix\$1\t\$2"
end

complete -x -c zig -n "not __fish_seen_subcommand_from env help targets version zen" -s h -l help -d "Print command-specific usage"

# Commands (ref: <https://github.com/ziglang/zig/blob/db890dbae72bc31e50d4ec641f2afce683df772d/src/main.zig#L71>)
complete -x -c zig -n __fish_use_subcommand -a build -d "Build project from build.zig"
complete -x -c zig -n __fish_use_subcommand -a fetch -d "Copy a package into global cache and print its hash"
complete -x -c zig -n __fish_use_subcommand -a init -d "Initialize a Zig package in the current directory"

complete -x -c zig -n __fish_use_subcommand -a build-exe -d "Create executable from source or object files"
complete -x -c zig -n __fish_use_subcommand -a build-lib -d "Create library from source or object files"
complete -x -c zig -n __fish_use_subcommand -a build-obj -d "Create object from source or object files"
complete -x -c zig -n __fish_use_subcommand -a test -d "Perform unit testing"
complete -x -c zig -n __fish_use_subcommand -a run -d "Create executable and run immediately"

complete -x -c zig -n __fish_use_subcommand -a ast-check -d "Look for simple compile errors in any set of files"
complete -x -c zig -n __fish_use_subcommand -a fmt -d "Reformat Zig source into canonical form"
complete -x -c zig -n __fish_use_subcommand -a reduce -d "Minimize a bug report"
complete -x -c zig -n __fish_use_subcommand -a translate-c -d "Convert C code to Zig code"

complete -x -c zig -n __fish_use_subcommand -a ar -d "Use Zig as a drop-in archiver"
complete -x -c zig -n __fish_use_subcommand -a cc -d "Use Zig as a drop-in C compiler"
complete -x -c zig -n __fish_use_subcommand -a c++ -d "Use Zig as a drop-in C++ compiler"
complete -x -c zig -n __fish_use_subcommand -a dlltool -d "Use Zig as a drop-in dlltool.exe"
complete -x -c zig -n __fish_use_subcommand -a lib -d "Use Zig as a drop-in lib.exe"
complete -x -c zig -n __fish_use_subcommand -a ranlib -d "Use Zig as a drop-in ranlib"
complete -x -c zig -n __fish_use_subcommand -a objcopy -d "Use Zig as a drop-in objcopy"
complete -x -c zig -n __fish_use_subcommand -a rc -d "Use Zig as a drop-in rc.exe"

complete -x -c zig -n __fish_use_subcommand -a env -d "Print lib path, std path, cache directory, and version"
complete -x -c zig -n __fish_use_subcommand -a help -d "Print help for `zig`"
complete -x -c zig -n __fish_use_subcommand -a std -d "View standard library documentation in a browser"
complete -x -c zig -n __fish_use_subcommand -a libc -d "Display native libc paths file or validate one"
complete -x -c zig -n __fish_use_subcommand -a targets -d "List available compilation targets"
complete -x -c zig -n __fish_use_subcommand -a version -d "Print version number"
complete -x -c zig -n __fish_use_subcommand -a zen -d "Print Zen of Zig"

# Command-specific options

## Steps
complete -x -c zig -n "__fish_seen_subcommand_from build && __fish_prev_arg_in build" -a "
    install\t'Copy build artifacts to prefix path (default)'
    uninstall\t'Remove build artifacts from prefix path'
    run\t'Run the app'
    test\t'test the executable'
    unzip\t'Build/install the unzip cmdline tool'
    "

## General options (ref: <https://github.com/ziglang/zig/blob/db890dbae72bc31e50d4ec641f2afce683df772d/lib/compiler/build_runner.zig#L1088>)
complete -r -c zig -n "__fish_seen_subcommand_from build" -s p -l prefix -d "Where to install files (default: zig-out)"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l prefix-lib-dir -d "Where to install libraries"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l prefix-exe-dir -d "Where to install executables"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l prefix-include-dir -d "Where to install C header files"
complete -x -c zig -n "__fish_seen_subcommand_from build" -l release -d "Request release mode" -a "fast safe small"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fdarling -d "Integrate with system-installed Darling"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-darling -d "Don't integrate with system-installed Darling (default)"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fqemu -d "Integrate with system-installed QEMU"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-qemu -d "Don't integrate with system-installed QEMU (default)"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l glibc-runtimes -d "Enhances QEMU integration"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o frosetta -d "Rely on Rosetta"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-rosetta -d "Don't rely on Rosetta (default)"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fwasmtime -d "Integrate with system-installed Wasmtime"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-wasmtime -d "Don't integrate with system-installed Wasmtime (default)"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fwine -d "Integrate with system-installed Wine"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-wine -d "Don't integrate with system-installed Wine (default)"

complete -f -c zig -n "__fish_seen_subcommand_from build" -s l -l list-steps -d "Print available steps"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose -d "Print commands before executing them"
complete -x -c zig -n "__fish_seen_subcommand_from build" -l color -a "auto off on" -d "Enable/Disable colored error messages"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l prominent-compile-errors -d "Output human-readable compile errors"
complete -x -c zig -n "__fish_seen_subcommand_from build" -l summary -d "Control the printing of the build summary" -a "
    failures\t'(Default) Only print failed steps'
    all\t'Print the build summary in its entirety'
    new\t'Omit cached steps'
    none\t'Do not print the build summary'
    "
complete -f -c zig -n "__fish_seen_subcommand_from build" -s j -d "Limit concurrent jobs (default is to use all CPU cores)"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l maxrss -d "Limit memory usage (default is to use available memory)"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l skip-oom-steps -d "skip steps that would exceed --maxrss"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l fetch -d "Exit after fetching dependency tree"

## Project-specific options
complete -x -c zig -n "__fish_seen_subcommand_from build" -o Dtarget -d "Specify the compilation target"
complete -x -c zig -n "__fish_seen_subcommand_from build" -o Dcpu -d "Specify CPU features to add/subtract"
complete -r -c zig -n "__fish_seen_subcommand_from build" -o Ddynamic-linker -d "Path to interpreter on the target system"
complete -x -c zig -n "__fish_seen_subcommand_from build" -o Doptimize -a "Debug ReleaseSafe ReleaseFast ReleaseSmall" -d "Optimizations on and safety on"

## System Integration Options
complete -r -c zig -n "__fish_seen_subcommand_from build" -l search-prefix -d "Add a path to look for binaries, libraries, headers"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l sysroot -d "Set the system root directory (usually /)"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l libc -d "Provide a file which specifies libc paths"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l system -d "Disable package fetching; enable all integrations"

## Advanced options
complete -x -c zig -n "__fish_seen_subcommand_from build" -o freference-trace -d "lines of reference trace shown per compile error"
complete -f -c zig -n "__fish_seen_subcommand_from build" -o fno-reference-trace -d "Disable reference trace"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l build-file -d "Override path to `build.zig`"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l cache-dir -d "Override path to zig cache directory"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l global-cache-dir -d "Override path to global Zig cache directory"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l zig-lib-dir -d "Override path to Zig lib directory"
complete -r -c zig -n "__fish_seen_subcommand_from build" -l build-runner -d "Override path to build runner"
complete -x -c zig -n "__fish_seen_subcommand_from build" -l seed -d "For shuffling dependency traversal order (default: random)"
complete -x -c zig -n "__fish_seen_subcommand_from build" -l debug-log -d "Enable debugging the compiler"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l debug-pkg-config -d "Fail if unknown pkg-config flags encountered"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-link -d "Enable compiler debug output for linking"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-air -d "Enable compiler debug output for Zig AIR"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-llvm-ir -d "Enable compiler debug output for LLVM IR"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-llvm-bc -d "Enable compiler debug output for LLVM BC"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-cimport -d "Enable compiler debug output for C imports"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-cc -d "Enable compiler debug output for C compilation"
complete -f -c zig -n "__fish_seen_subcommand_from build" -l verbose-llvm-cpu-features -d "Enable compiler debug output for LLVM CPU features"

complete -x -c zig -n "__fish_seen_subcommand_from ast-check" -l color -a "auto off on" -d "Enable/Disable colored error messages"
complete -c zig -n "__fish_seen_subcommand_from ast-check" -s t -d "Output ZIR in text form to stdout"

set -l zig_build_generic_commands build-exe build-lib build-obj run test translate-c

## General options <https://github.com/ziglang/zig/blob/db890dbae72bc31e50d4ec641f2afce683df772d/src/main.zig#L366>
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l watch -d "Enable compiler REPL"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l color -a "auto off on" -d "Enable/Disable colored error messages"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-bin -d "Output machine code (default)"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-bin -d "Don't output machine code"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-asm -d "Output assembly code"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-asm -d "Don't output assembly code (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-llvm-ir -d "Produce a file containing LLVM IR"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-llvm-ir -d "Don't produce a file containing LLVM IR (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-llvm-bc -d "Produce a file containing LLVM bitcode"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-llvm-bc -d "Don't produce a file containing LLVM bitcode (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-h -d "Generate a C header file"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-h -d "Don't generate a C header file (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-docs -d "Create the documentation"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-docs -d "Don't produce the documentation (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-analysis -d "Write analysis JSON file"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-analysis -d "Don't write analysis JSON file (default)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o femit-implib -d "Produce an import library file (default)"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-emit-implib -d "Don't produce an import library file"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l show-builtin -d "Output the source of `@import(\"builtin\")`"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l cache-dir -d "Override the local cache directory"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l global-cache-dir -d "Override the global cache directory"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l zig-lib-dir -d "Override path to Zig installation lib directory"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l enable-cache -d "Output to cache directory"

## Compile options
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o target -d "Specify the compilation target"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o mcpu -d "Specify target CPU and feature set"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o mcmodel -a "default tiny small kernel medium large" -d "Limit range of code and data virtual addresses"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o mred-zone -d "Force-enable the \"red-zone\""
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o mno-red-zone -d "Force-disable the \"red-zone\""
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fomit-frame-pointer -d "Omit the stack frame pointer"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-omit-frame-pointer -d "Store the stack frame pointer"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o mexec-model -d "Execution model (WASI)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l name -d "Override root name"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s O -a "
    Debug\t'Optimizations off, safety on (default)'
    ReleaseFast\t'Optimizations on, safety off'
    ReleaseSafe\t'Optimizations on, safety on'
    ReleaseSmall\t'Optimize for small binary, safety off'
    " -d "Choose what to optimize for"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l pkg-begin -d "Make pkg available to import and push current pkg"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l pkg-end -d "Pop current pkg"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l main-pkg-path -d "Set the directory of the root package"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fPIC -d "Force-enable Position Independent Code"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-PIC -d "Force-disable Position Independent Code"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fPIE -d "Force-enable Position Independent Executable"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-PIE -d "Force-disable Position Independent Executable"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o flto -d "Force-enable Link Time Optimization"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-lto -d "Force-disable Link Time Optimization"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fstack-check -d "Enable stack probing in unsafe builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-stack-check -d "Disable stack probing in safe builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fsanitize-c -d "Enable C undefined behavior detection in unsafe builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-sanitize-c -d "Disable C undefined behavior detection in safe builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fvalgrind -d "Include valgrind client requests in release builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-valgrind -d "Omit valgrind client requests in debug builds"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fsanitize-thread -d "Enable Thread Sanitizer"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-sanitize-thread -d "Disable Thread Sanitizer"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fdll-export-fns -d "Mark exported functions as DLL exports (Windows)"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-dll-export-fns -d "Force-disable marking exported functions as DLL exports"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o funwind-tables -d "Always produce unwind table entries for all functions"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-unwind-tables -d "Never produce unwind table entries"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fLLVM -d "Force using LLVM"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-LLVM -d "Prevent using LLVM"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fClang -d "Force using Clang"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-Clang -d "Prevent using Clang"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fstage1 -d "Force using bootstrap compiler"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-stage1 -d "Prevent using bootstrap compiler"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fsingle-threaded -d "Code assumes there is only one thread"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-single-threaded -d "Code may not assume there is only one thread"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l strip -d "Omit debug symbols"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o ofmt -a "
    elf\tELF
    c\t'C source code'
    wasm\tWasm
    coff\t'COFF (Windows)'
    macho\tMach-O
    spirv\tSPIR-V
    plan9\t'Plan 9 object format'
    hex\t'Intel hex'
    raw\t'Dump machine code directly'
    " -d "Override target object format"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o dirafter -d "Add directory to AFTER include search path"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o isystem -d "Add directory to SYSTEM include search path"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s I -d "Add directory to include search path"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s D -d "Define the C macro"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l libc -d "Provide a file which specifies libc paths"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o cflags -d "Set extra flags for the next positional C source files"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o ffunction-sections -d "Places each function in a separate section"

## Link options
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s l -l library -d "Link against system library (only if actually used)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o needed-l -l needed-library -d "Link against system library (even if unused)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s L -l library-directory -d "Add a directory to the library search path"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s T -l script -d "Use a custom linker script"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l version-script -d "Provide a version .map file"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l dynamic-linker -d "Set the dynamic interpreter path"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l sysroot -d "Set the system root directory"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l version -d "Dynamic library semver"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l entry -d "Set the entrypoint symbol name"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fsoname -d "Override the default SONAME value"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-soname -d "Disable emitting a SONAME"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fLLD -d "Force using LLD"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-LLD -d "Prevent using LLD"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fcompiler-rt -d "Always include compiler-rt symbols"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-compiler-rt -d "Prevent including compiler-rt symbols"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o rdynamic -d "Add all symbols to the dynamic symbol table"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o rpath -d "Add directory to the runtime library search path"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o feach-lib-rpath -d "Ensure adding rpath for each used dynamic library"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-each-lib-rpath -d "Prevent adding rpath for each used dynamic library"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fallow-shlib-undefined -d "Allows undefined symbols in shared libraries"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fno-allow-shlib-undefined -d "Disallows undefined symbols in shared libraries"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l eh-frame-hdr -d "Enable C++ exception handling by passing `--eh-frame-hdr` to linker"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l emit-relocs -d "Enable output of relocation sections for post build tools"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s z -a "
    nodelete\t'Indicate that the object cannot be deleted from a process'
    notext\t'Permit read-only relocations in read-only segments'
    defs\t'Force a fatal error if any undefined symbols remain'
    origin\t'Indicate that the object must have its origin processed'
    noexecstack\t'Indicate that the object requires an executable stack'
    now\t'Force all relocations to be processed on load'
    relro\t'Force all relocations to be resolved and be read-only on load'
    " -d "Set linker extension flags"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o dynamic -d "Force output to be dynamically linked"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o static -d "Force output to be statically linked"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o Bsymbolic -d "Bind global references locally"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l subsystem -a "
    console\t'Win32 console application'
    windows\t'Windows application'
    posix\t'Application that runs with the POSIX subsystem'
    native\t'Kernel mode drivers'
    efi_application\t'The EFI subsystem'
    efi_boot_service_driver\t'The EFI subsystem'
    efi_rom\t'The EFI subsystem'
    efi_runtime_driver\t'The EFI subsystem'
    " -d "The Windows subsystem to the linker (Windows)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l stack -d "Override default stack size"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l image-base -d "Set base address for executable image"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o framework -d "Link against framework (Darwin)"
complete -r -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -s F -d "Add search path for frameworks (Darwin)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o install_name -d "Add dylib's install name (Darwin)"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l import-memory -d "Import memory from the environment (Wasm)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l initial-memory -d "Initial size of the linear memory (Wasm)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l max-memory -d "Maximum size of the linear memory (Wasm)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l global-base -d "Where to start to place global data (Wasm)"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l export -d "Force a symbol to be exported (Wasm)"

## Test options
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-filter -d "Skip tests that do not match filter"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-name-prefix -d "Add prefix to all tests"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-cmd -d "Specify test execution command"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-cmd-bin -d "Appends test binary path to `--test-cmd`"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-evented-io -d "Runs the test in evented I/O mode"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l test-no-exec -d "Compiles test binary without running it"

## Debug options
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o ftime-report -d "Print timing diagnostics"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -o fstack-report -d "Print stack size diagnostics"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-link -d "Display linker invocations"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-cc -d "Display C compiler invocations"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-air -d "Enable compiler debug output for Zig AIR"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-mir -d "Enable compiler debug output for Zig MIR"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-llvm-ir -d "Enable compiler debug output for LLVM IR"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-cimport -d "Enable compiler debug output for C imports"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l verbose-llvm-cpu-features -d "Enable compiler debug output for LLVM CPU features"
complete -x -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l debug-log -d "Enable printing debug/info log messages for scope"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l debug-compile-errors -d "Crash with helpful diagnostics at the first compile error"
complete -c zig -n "__fish_seen_subcommand_from $zig_build_generic_commands" -l debug-link-snapshot -d "Enable dumping of the linker's state in JSON"

complete -x -c zig -n "__fish_seen_subcommand_from fmt" -l color -a "auto off on" -d "Enable/Disable colored error messages"
complete -c zig -n "__fish_seen_subcommand_from fmt" -l stdin -d "Use stdin and stdout for I/O"
complete -c zig -n "__fish_seen_subcommand_from fmt" -l check -d "Check if the input is formatted"
complete -c zig -n "__fish_seen_subcommand_from fmt" -l ast-check -d "Run `zig ast-check` on every file"

complete -c zig -n "__fish_seen_subcommand_from cc c++" -a "(__fish_complete_zig_cc_and_cpp)"

complete -x -c zig -n "__fish_seen_subcommand_from libc" -o target -d "Specify the compilation target"
