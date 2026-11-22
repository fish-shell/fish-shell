__fish_cargo_helper

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
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl target -d "Build for target" -xa "(__cargo_targets)"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl asm-style -d "ASM style (default: intel)" -xa "intel att"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl build-type -d "Build type (default: release)" -xa "debug release"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl features -d "Cargo features to enable" -xa "(__cargo_features)"
complete -c cargo -n "__fish_seen_subcommand_from asm" -rl manifest-path -d "Run cargo-asm in a different directory"

# Dynamically generate completions for the function/impl path to translate to asm (the reason these completions exist)
# Warning: this will build the project and can take time! We make sure to only call it if it's not a switch so completions
# for --foo will always be fast.
if command -q timeout
    complete -c cargo -n "__fish_seen_subcommand_from asm; and not __fish_is_switch" -xa "(__cargo_wrapping={timeout,1} __cargo asm)"
else
    complete -c cargo -n "__fish_seen_subcommand_from asm; and not __fish_is_switch" -xa "(__cargo asm)"
end
