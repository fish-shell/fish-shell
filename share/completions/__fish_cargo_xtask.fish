# Completions for `cargo xtask` are a tricky problem,
# due to several factors:
# - The appropriate completions depend on the current working directory. Different projects have different xtasks.
# - Even within a single project, what `cargo xtask` does can change frequently, requiring updates to the completions.
# - It needs completions for a sub-command, which fish does not have great native support for.
# All of these problems can be circumvented by defining a custom command name for each project,
# which simply maps to `cargo xtask`.
# E.g. for fish, we define in `share/functions/__fish_cargo_xtask.fish`:
#
# function __fish_cargo_xtask
#     cargo xtask $argv
# end
#
# Once we have such a custom command for a particular project, we can define completions for it.
# These could be traditional static completions, or the more versatile dynamic completions supported e.g. by clap.
# For fish's xtasks, we use dynamic completions:
COMPLETE=fish __fish_cargo_xtask 2>/dev/null | source
# This uses clap's dynamic completion implementation.
# Note that this only works because we tell clap that the `bin_name` is `__fish_cargo_xtask`.
# This will also result in that name being shown in clap's help output, improving discoverability.
# For other projects, doing something analogous might not be desirable.
# Instead, the following approach can be used, still assuming that clap's dynamic completion approach is used,
# but without using a custom `bin_name`.
#
# complete --keep-order --exclusive --command your-custom-command --arguments "(
#     COMPLETE=fish cargo xtask -- (commandline --current-process --tokenize --cut-at-cursor) (commandline --current-token) 2>/dev/null
# )"
#
# This command allows separately specifying your alias and the backing command.
# Otherwise, it is mostly what the previous above would expand to, with minor tweaks to improve it.
# These tweaks are unnecessary when using the approach above, but useful when using the manual expansion.
# Namely, the automatic expansion would use the path to the xtask executable instead of `cargo xtask`,
# which does not take into account that the executable could have been deleted, or be outdated.
# Additionally, redirecting stderr to /dev/null avoids ugly artifacts of Cargo's build/run output being shown.
#
# If you use xtasks in multiple projects, it can be cumbersome to remember which alias you use in each.
# To avoid having to do so, and also to help reduce the amount of typing and improve the potential for muscle memory,
# we recommend using an `abbr` command that expands to one of your custom aliases depending on the working directory,
# or to just `cargo xtask` if no custom command exists for the directory.
# How the directory detection is done depends on your specific setup.
# A simple approach, which assumes that the fish workspace can be identified by looking for `fish-shell` in any path component,
# is shown here:
#
# function fish_expand_cx
#     if string split / -- $PWD | grep --quiet '^fish-shell$'
#         echo __fish_cargo_xtask
#     else
#         echo "cargo xtask"
#     end
# end
# abbr --add cx --function fish_expand_cx
#
# If you add this to your config, typing `cx ` will expand to `__fish_cargo_xtask `
# if `fish-shell` appears as a component of the current working directory,
# or to `cargo xtask` otherwise.
# Support for more projects could be added by adding `else if` branches, for example.
# Instead of scripting this in fish, you could also invoke a custom command which prints the appropriate expansion.
