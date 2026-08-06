function __cargo
    set -l tmp $__cargo_wrapping cargo --color=never $argv
    RUSTUP_AUTO_INSTALL=0 $tmp
end

# If using rustup, get the list of installed targets from there. Otherwise print all targets.
#
# NB: I wasn't sure if it's possible to manually target a platform you don't have the corresponding toolchain for installed,
# and it turns out indeed this isn't strictly correct if you choose to manually compile the standard library (-Zbuild-std,
# nightly only) and are targeting a platform that your native linker also supports, e.g.
# `cargo build +nightly -Zbuild-std --target=i586-unknown-linux-gnu` works even if you only have the i686-unknown-linux-gnu
# toolchain installed.
#
# Ideally, we'd use rustup's "installed targets" but fall back to completions from rustc's "all targets" list, but we don't
# have an easy way to do that in the `complete` machinery at this time.
function __cargo_targets
    if command -q rustup
        rustup target list | string replace -rf "^(\S+) \(installed\)" '$1'
    else
        rustc --print target-list
    end
end

function __cargo_features
    if command -q jq
        __cargo read-manifest | jq -r '.features | keys | .[]' | __fish_concat_completions
    else if set -l python (__fish_anypython)
        __cargo read-manifest | command $python -Sc "import sys, json"\n"print(*json.load(sys.stdin)['features'].keys(), sep='\n')" | __fish_concat_completions
    end
end

function __cargo_packages
    if command -q jq
        __cargo metadata --no-deps --format-version 1 | jq -r '.packages | .[] | .name' | __fish_concat_completions
    else if set -l python (__fish_anypython)
        __cargo metadata --no-deps --format-version 1 |
            command $python -Sc "import sys, json"\n"print(*[x['name'] for x in json.load(sys.stdin)['packages']], sep='\n')"
    end
end

function __fish_cargo_helper
    # dummy function to allow sourcing this file
end
