# localization: skip(private)

# This command is intended to be used instead of `cargo xtask` in fish workspaces to opt into dynamic completions.
# See `share/completions/__fish_cargo_xtask.fish` for documentation.
function __fish_cargo_xtask
    cargo xtask $argv
end
