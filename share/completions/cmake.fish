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

complete -c cmake -n "not __fish_cmake_has_any_subcommand; and __fish_is_nth_token 1" -rfa "(__fish_complete_directories)"
complete -c cmake -n "not __fish_cmake_has_any_subcommand; and not __fish_is_nth_token 1" -f
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s S -rF -d "Explicitly specify a source directory"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s B -rF -d "Explicitly specify a build directory"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s G -rf -a "'Unix Makefiles' Ninja 'Ninja Multi-Config' FASTBuild 'Watcom WMake' 'CodeBlocks - Ninja' 'CodeBlocks - Unix Makefiles' 'CodeLite - Ninja' 'CodeLite - Unix Makefiles' 'Eclipse CDT4 - Ninja' 'Eclipse CDT4 - Unix Makefiles' 'Kate - Ninja' 'Kate - Ninja Multi-Config' 'Kate - Unix Makefiles' 'Sublime Text 2 - Ninja' 'Sublime Text 2 - Unix Makefiles'" -d "Specify a build system generator"
complete -c cmake -n "not __fish_cmake_has_any_subcommand" -s T -rf -d "Specify toolset name if supported by generator"

complete -c cmake -n "__fish_is_first_arg" -l build -rF -d "Build a CMake-generated project binary tree"
complete -c cmake -n "__fish_is_first_arg" -l install -rF -d "Install a CMake-generated project binary tree"

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
