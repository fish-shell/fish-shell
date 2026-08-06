function __fish_cargo
    set -l tmp $__fish_cargo_wrapping cargo --color=never $argv
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
function __fish_cargo_targets
    if command -q rustup
        rustup target list | string replace -rf "^(\S+) \(installed\)" '$1'
    else
        rustc --print target-list
    end
end

function __fish_cargo_features
    if command -q jq
        __fish_cargo read-manifest | jq -r '.features | keys | .[]' | __fish_concat_completions
    else if set -l python (__fish_anypython)
        __fish_cargo read-manifest | command $python -Sc "import sys, json"\n"print(*json.load(sys.stdin)['features'].keys(), sep='\n')" | __fish_concat_completions
    end
end

# Flags (no parameters)
complete -c cargo -n "__fish_seen_subcommand_from asm" -l comments -d "Print asm comments"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l debug-info -d "Generate asm w/ debug info even if not required"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l debug-mode -d "Print output useful for debugging"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l directives -d "Print asm directives"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l help -s h -d "Print cargo-asm help info"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l json -d "Serialize asm AST to JSON"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l lib -d "Build only the lib target"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l no-color -d "Disable color output"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l no-default-features -d "Disable all cargo features on build"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l rust -d "Interleave asm output w/ rust code"
complete -c cargo -n "__fish_seen_subcommand_from asm" -l version -s V -d "Print cargo-asm version info"

# Options (require a parameter)
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl target -d "Build for target" -xa "(__fish_cargo_targets)"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl asm-style -d "ASM style (default: intel)" -xa "intel att"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl build-type -d "Build type (default: release)" -xa "debug release"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl features -d "Cargo features to enable" -xa "(__fish_cargo_features)"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl manifest-path -d "Run cargo-asm in a different directory"

# Dynamically generate completions for the function/impl path to translate to asm (the reason these completions exist)
# Warning: this will build the project and can take time! We make sure to only call it if it's not a switch so completions
# for --foo will always be fast.
if command -q timeout
    complete -c cargo -n "__fish_seen_subcommand_from asm; and not __fish_is_switch" -xa "(__fish_cargo_wrapping={timeout,1} __fish_cargo asm)"
else
    complete -c cargo -n "__fish_seen_subcommand_from asm; and not __fish_is_switch" -xa "(__fish_cargo asm)"
end
