# Completions for the archlinux package information and build tool `asp` (https://github.com/falconindy/asp)
set -l commands checkout difflog export gc disk-usage help list-{all,arches,local,repos} {short,}log show update untrack

complete -c asp -n "not __fish_seen_subcommand_from $commands" -a checkout -d "Checkout package" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a difflog -d "Show the full history of the target, with file diffs" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a export -d "Put the source files in current directory" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a gc -d "Collect garbage" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a disk-usage -d "Report approximate disk usage" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a help -d "Display help and exit" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a list-all -d "List all known packages" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a list-arches -d "List the architectures the targets are available for" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a list-local -d "List locally tracked packages" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a list-repos -d "List repos the targets exist in" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a log -d "Show revision history of the target" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a shortlog -d "Show condensed revision history" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a show -d "Show the target's PKGBUILD" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a update -d "Update given targets" -f
complete -c asp -n "not __fish_seen_subcommand_from $commands" -a untrack -d "Remove target from local repository" -f

# Remove pointless "packages/" or "community/" before package names
# Don't show foreign packages for untrack, and show no packages at all for gc, help, disk-usage, list-{all,local}
# This will run into the description race.
complete -c asp -n "__fish_seen_subcommand_from checkout {diff,short,}log export list-{arches,repos} show update" -a "(asp list-all | string replace -r '.*/' '')" -f
complete -c asp -n "__fish_seen_subcommand_from checkout {diff,short,}log export list-{arches,repos} show update untrack" -a "(asp list-local | string replace -r '.*/' '')" -f \
-d "Locally tracked package"

