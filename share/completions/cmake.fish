# Based on version 4.4.4.

# cmake --help
# cmake --build ?
# cmake --install ?

set -l subcommands "--build" "--install"

function __fish_cmake_has_any_subcommand -V subcommands
    set -l first (commandline -xpc)[2]
    set -q first[1]
    contains -- $first $subcommands
end


function __fish_cmake_has_subcommand --argument-names subcommand
    set -l first (commandline -xpc)[2]
    set -q first[1]
    and string match -q -- "--$subcommand" $first
end

function __fish_cmake_generators
    # Rely on `-E capabilities` instead of `--help` for stability.
    # Only provide generators if we have a way to parse JSON.
    # Extra generators are combined with their base generator, e.g. `CodeBlocks - Ninja`.
    if command -q jq
        cmake -E capabilities | jq -r '.generators[] | .name, (.name as $name | .extraGenerators[] | "\(.) - \($name)")'
    else if set -l python (__fish_anypython)
        cmake -E capabilities | command $python -Sc "import sys, json
for g in json.load(sys.stdin)['generators']:
    print(g['name'])
    for e in g['extraGenerators']:
        print(e + ' - ' + g['name'])"
    end
end

function __fish_cmake_help_keywords
    for kind in command diagnostic module policy property variable
        # Attach `kind` as a description.
        cmake --help-$kind-list | string replace -r '$' \t$kind
    end
    # Manuals are output as `name(section)`, but accepted as `name.section`.
    # E.g. `cmake(1)` -> `cmake.1`.
    cmake --help-manual-list | string replace -r '\((\d+)\)$' '.$1'\tmanual
end

complete -c cmake -n "not __fish_cmake_has_any_subcommand; and __fish_is_nth_token 1" -rfa "(__fish_complete_directories)"
complete -c cmake -n "not __fish_cmake_has_any_subcommand; and not __fish_is_nth_token 1" -f
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s S -rF -d "Explicitly specify a source directory"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s B -rF -d "Explicitly specify a build directory"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s G -rf -a "(__fish_cmake_generators)" -d "Specify a build system generator"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s T -rf -d "Specify toolset name if supported by generator"

complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l fresh -d "Configure a fresh build tree, removing any existing cache file"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s N -d "View mode only"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s P -rF -d "Process script mode"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l find-package -d "Legacy pkg-config like mode. Do not use"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l graphviz -rF -d "Generate graphviz of dependencies"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l system-information -rF -d "Dump information about this system"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l print-config-dir -d "Print CMake config directory for user-wide FileAPI queries"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l log-level -rf -a "ERROR WARNING NOTICE STATUS VERBOSE DEBUG TRACE" -d "Set the verbosity of messages from CMake files"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l log-context -d "Prepend log messages with context, if given"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l debug-trycompile -d "Do not delete the try_compile build tree"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l debug-output -d "Put cmake in a debug mode"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l debug-find -d "Put cmake find in a debug mode"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l debug-find-pkg -rf -d "Limit cmake debug-find to the comma-separated list of packages"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l debug-find-var -rf -d "Limit cmake debug-find to the comma-separated list of result variables"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l trace -d "Put cmake in trace mode"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l trace-expand -d "Put cmake in trace mode with variable expansion"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l trace-format -rf -a "human json-v1" -d "Set the output format of the trace"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l trace-source -rF -d "Trace only this CMake file/module"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l trace-redirect -rF -d "Redirect trace output to a file instead of stderr"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l check-system-vars -d "Find problems with variable usage in system files"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l compile-no-warning-as-error -d "Ignore COMPILE_WARNING_AS_ERROR property and variable"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l link-no-warning-as-error -d "Ignore LINK_WARNING_AS_ERROR property and variable"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l profiling-format -rf -a google-trace -d "Output data for profiling CMake scripts"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l profiling-output -rF -d "Select an output path for the profiling data"

# `--help` and `--version` commands also have an optional <file> argument,
# we don't complete it as it's rarely used and better to keep things simple.
# `--version` also has `/V` and `/version` options - ignore them as unneeded alternatives.
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l version -f -a json-v1 -d "Print version number and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s h -l help -rf -a "(__fish_cmake_help_keywords)" -d "Print help for one keyword and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-full -d "Print all help manuals and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-manual -rf -a "(cmake --help-manual-list | string replace -r '\(\d+\)\$' '')" -d "Print one help manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-manual-list -d "List help manuals available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-command -rf -a "(cmake --help-command-list)" -d "Print help for one command and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-command-list -d "List commands with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-commands -d "Print cmake-commands manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-diagnostic -rf -a "(cmake --help-diagnostic-list)" -d "Print help for one diagnostic and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-diagnostic-list -d "List diagnostics with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-diagnostics -d "Print cmake-diagnostics manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-module -rf -a "(cmake --help-module-list)" -d "Print help for one module and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-module-list -d "List modules with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-modules -d "Print cmake-modules manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-policy -rf -a "(cmake --help-policy-list)" -d "Print help for one policy and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-policy-list -d "List policies with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-policies -d "Print cmake-policies manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-property -rf -a "(cmake --help-property-list)" -d "Print help for one property and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-property-list -d "List properties with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-properties -d "Print cmake-properties manual and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-variable -rf -a "(cmake --help-variable-list)" -d "Print help for one variable and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-variable-list -d "List variables with help available and exit"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -l help-variables -d "Print cmake-variables manual and exit"

complete -c cmake -n "__fish_is_first_arg" -l build -rF -d "Build a CMake-generated project binary tree"
complete -c cmake -n "__fish_is_first_arg" -l install -rF -d "Install a CMake-generated project binary tree"
complete -c cmake -n "__fish_is_first_arg" -l open -rfa "(__fish_complete_directories)" -d "Open generated project in the associated application"

# TODO: consider preset.
# # TODO: consider -- ?
complete -c cmake -n "__fish_cmake_has_subcommand build; and __fish_is_nth_token 1" -rfa "(__fish_complete_directories)"
complete -c cmake -n "__fish_cmake_has_subcommand build; and not __fish_is_nth_token 1" -f
complete -c cmake -n "__fish_cmake_has_subcommand build" -l presets -rf -d "Specify a build preset"
complete -c cmake -n "__fish_cmake_has_subcommand build" -l presets-file -rF -d "Specify the path to a presets file"
complete -c cmake -n "__fish_cmake_has_subcommand build" -l list-presets -F -d "List available presets"
complete -c cmake -n "__fish_cmake_has_subcommand build" -s j -l parallel -rf -d "Build in parallel using the given number of jobs"
complete -c cmake -n "__fish_cmake_has_subcommand build" -s t -l target -rf -d "Build <tgt> instead of default targets"
complete -c cmake -n "__fish_cmake_has_subcommand build" -l config -rf -a "Release RelWithDebInfo MinRelSize Debug" -d "For multi-configuration tools, choose <cfg>"
complete -c cmake -n "__fish_cmake_has_subcommand build" -l clean-first -d "Build target 'clean' first, then build"
complete -c cmake -n "__fish_cmake_has_subcommand build" -l resolve-package-references -rf -a "on only off" -d "Restore/resolve package references during build."
complete -c cmake -n "__fish_cmake_has_subcommand build" -s v -l verbose -d "Enable verbose output, if supported"

complete -c cmake -n "__fish_cmake_has_subcommand install; and __fish_is_nth_token 1" -rfa "(__fish_complete_directories)"
complete -c cmake -n "__fish_cmake_has_subcommand install; and not __fish_is_nth_token 1" -f
complete -c cmake -n "__fish_cmake_has_subcommand install" -l config -rf -a "Release RelWithDebInfo MinRelSize Debug" -d "For multi-configuration tools, choose <cfg>"
complete -c cmake -n "__fish_cmake_has_subcommand install" -l component -rf -d "Component-based install. Only install <comp>"
complete -c cmake -n "__fish_cmake_has_subcommand install" -l default-directory-permissions -rf -d "Default install permission"
complete -c cmake -n "__fish_cmake_has_subcommand install" -l prefix -rfa "(__fish_complete_directories)" -d "The installation prefix CMAKE_INSTALL_PREFIX"
complete -c cmake -n "__fish_cmake_has_subcommand install" -l strip -d "Performing install/strip"
complete -c cmake -n "__fish_cmake_has_subcommand install" -s v -l verbose -d "Enable verbose output"
