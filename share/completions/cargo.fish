# Tab completion for cargo (https://github.com/rust-lang/cargo).
complete -c cargo -s h -l help
complete -c cargo -s V -l version -d 'Print version info and exit'
complete -c cargo -l list -d 'List installed commands'
complete -c cargo -s v -l verbose -d 'Use verbose output'
complete -c cargo -s q -l quiet -d 'No output printed to stdout'

set -l __fish_cargo_subcommands (cargo --list 2>&1 | string replace -rf '^\s+([^\s]+)\s+(.*)' '$1\t$2' | string escape)

complete -c cargo -f -c cargo -n __fish_use_subcommand -a "$__fish_cargo_subcommands"
complete -c cargo -x -c cargo -n '__fish_seen_subcommand_from help' -a "$__fish_cargo_subcommands"

for x in bench build clean doc fetch generate-lockfile \
    locate-project package pkgid publish \
    read-manifest run rustc test update \
    verify-project
    complete -c cargo -r -n "__fish_seen_subcommand_from $x" -l manifest-path -d 'path to the manifest to compile'
end

for x in bench build clean doc rustc test update
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -s p -l spec -d 'Package to build'
end

for x in bench build clean doc run rustc test
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l target -d 'Build for the target triple'
end

for x in bench build rustc test
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l bench -a "(cargo bench --bench 2>&1 | string replace -rf '^\s+' '')"
    complete -c cargo -n "__fish_seen_subcommand_from $x" -l lib -d 'Only this package\'s library'
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l test -a "(cargo test --test 2>&1 | string replace -rf '^\s+' '')"
end

function __list_cargo_examples
    if not test -d ./examples
        return
    end

    find ./examples/ -mindepth 1 -maxdepth 1 -type f -name "*.rs" -or -type d \
        | string replace -r './examples/(.*)(?:.rs)?$' '$1'
end
for x in bench build run rustc test
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l bin -a "(cargo run --bin 2>&1 | string replace -rf '^\s+' '')"
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l example -a "(__list_cargo_examples)"
end

for x in build run rustc test
    complete -c cargo -n "__fish_seen_subcommand_from $x" -l release -d 'Build artifacts in release mode, with optimizations'
end

for x in bench test
    complete -c cargo -n "__fish_seen_subcommand_from $x" -l no-run -d 'Compile but do not run'
end

for x in bench build doc run rustc test
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -s j -l jobs -d 'Number of jobs to run in parallel'
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l features -d 'Space-separated list of features to also build'
    complete -c cargo -n "__fish_seen_subcommand_from $x" -l no-default-features -d 'Do not build the `default` feature'
end

complete -c cargo -n '__fish_seen_subcommand_from doc' -l no-deps -d 'Don\'t build documentation for dependencies'

complete -c cargo -x -n '__fish_seen_subcommand_from new' -l vcs -a 'none hg git' -d 'Specify a vcs to use'
complete -c cargo -x -n '__fish_seen_subcommand_from new' -l name

# This bin does not take any arguments which is why it is not defined above.
complete -c cargo -n '__fish_seen_subcommand_from new' -l bin

complete -c cargo -x -n '__fish_seen_subcommand_from git-checkout' -l url
complete -c cargo -x -n '__fish_seen_subcommand_from git-checkout' -l reference

for x in login publish search
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l host -d 'The host to submit the request to'
end

complete -c cargo -n '__fish_seen_subcommand_from doc' -l open -d 'Opens the docs in a browser after the operation'

complete -c cargo -r -n '__fish_seen_subcommand_from owner' -s a -l add -d 'Login of a user to add as an owner'
complete -c cargo -r -n '__fish_seen_subcommand_from owner' -s r -l remove -d 'Login of a user to remove as an owner'

for x in owner yank
    complete -c cargo -r -n "__fish_seen_subcommand_from $x" -l index -d 'Registry index to use'
end

for x in owner publish yank
    complete -c cargo -x -n "__fish_seen_subcommand_from $x" -l token -d 'API token to use when authenticating'
end

complete -c cargo -n '__fish_seen_subcommand_from package' -l no-verify -d 'Don\'t verify the contents by building them'
complete -c cargo -n '__fish_seen_subcommand_from package' -l no-metadata -d 'Ignore warnings about a lack of human-usable metadata'

complete -c cargo -n '__fish_seen_subcommand_from update' -l aggressive -d 'Force updating all dependencies of <name> as well'
complete -c cargo -x -n '__fish_seen_subcommand_from update' -l precise -d 'Update a single dependency to exactly PRECISE'

complete -c cargo -x -n '__fish_seen_subcommand_from yank' -l vers -d 'The version to yank or un-yank'
complete -c cargo -n '__fish_seen_subcommand_from yank' -l undo -d 'Undo a yank, putting a version back into the index'

function __fish_cargo_packages
    find . -name Cargo.toml | string replace -rf '.*/([^/]+)/?Cargo.toml' '$1'
end
complete -c cargo -n '__fish_seen_subcommand_from run test build debug check' -l package \
    -xa "(__fish_cargo_packages)"
