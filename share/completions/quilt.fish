# completions for quilt version 0.69

complete -c quilt -f

# quilt [command] -h
set -l commands (string match --all --regex '\S+' '
    add
    annotate
    applied
    delete
    diff
    edit
    files
    fold
    fork
    graph
    grep
    header
    import
    mail
    new
    next
    patches
    pop
    previous
    push
    refresh
    remove
    rename
    revert
    series
    setup
    snapshot
    top
    unapplied
    upgrade
')
complete -c quilt -n "__fish_seen_subcommand_from $commands" -s h -d 'Show help' -f

# quilt [--quiltrc file] [--trace] command [options]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l quiltrc -d 'Config file to use' -f
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l trace -d 'Trace internal shell commands (bash -x)' -f

# quilt --version
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l version -d 'Show version' -f

# quilt add [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a add -d 'Add files to patch' -f

# quilt annotate [-P patch] {file}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a annotate -d 'Show which patches modify which lines in file' -f

# quilt applied [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a applied -d 'List applied patches in the stack' -f

# quilt delete [-r] [--backup] [patch|-n]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a delete -d 'Remove patch from the series' -f

# quilt diff [-p n|-p ab] [-u|-U num|-c|-C num] [--combine patch|-z] [-R] [-P patch] [--snapshot] [--diff=utility] [--no-timestamps] [--no-index] [--sort] [--color[=always|auto|never]] [file ...]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a diff -d 'Diff files in patch' -f

# quilt edit file ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a edit -d 'Edit files in $EDITOR' -f

# quilt files [-v] [-a] [-l] [--combine patch] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a files -d 'Lists files changed by patch' -f

# quilt fold [-R] [-q] [-f] [-p strip-level]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a fold -d 'Integrate patch from stdin into the topmost patch' -f

# quilt fork [new_name]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a fork -d 'Fork the topmost patch' -f

# quilt graph [--all] [--reduce] [--lines[=num]] [--edge-labels=files] [-T ps] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a graph -d 'Visualize patch dependencies using dot' -f

# quilt grep [-h|options] {pattern}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a grep -d 'Search through source files' -f

# quilt header [-a|-r|-e] [--backup] [--strip-diffstat] [--strip-trailing-whitespace] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a header -d 'Print/change the header of patch' -f

# quilt import [-p num] [-R] [-P patch] [-f] [-d {o|a|n}] patchfile ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a import -d 'Import external patches' -f

# quilt mail {--mbox file|--send} [-m text] [-M file] [--prefix prefix] [--sender ...] [--from ...] [--to ...] [--cc ...] [--bcc ...] [--subject ...] [--reply-to message] [--charset ...] [--signature file] [first_patch [last_patch]]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a mail -d 'Create or send mail messages from patches' -f

# quilt new [-p n] {patchname}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a new -d 'Start a new topmost patch' -f

# quilt next [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a next -d 'Name of patch after the topmost/given patch' -f

# quilt patches [-v] [--color[=always|auto|never]] {file} [files...]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a patches -d 'List patches that modify the file(s)' -f

# quilt pop [-afRqv] [--refresh] [num|patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a pop -d 'Unapply patches from the stack' -f

# quilt previous [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a previous -d 'Name of patch before the topmost/given patch' -f

# quilt push [-afqvm] [--fuzz=N] [--merge[=merge|diff3]] [--leave-rejects] [--color[=always|auto|never]] [--refresh] [num|patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a push -d 'Apply patches from the series' -f

# quilt refresh [-p n|-p ab] [-u|-U num|-c|-C num] [-z[new_name]] [-f] [--no-timestamps] [--no-index] [--diffstat] [--sort] [--backup] [--strip-trailing-whitespace] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a refresh -d 'Update patch with changes from source' -f

# quilt remove [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a remove -d 'Remove files from the patch' -f

# quilt rename [-P patch] new_name
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a rename -d 'Rename the topmost/given patch' -f

# quilt revert [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a revert -d 'Revert file changes in patch' -f

# quilt series [--color[=always|auto|never]] [-v]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a series -d 'List all patches in the series' -f

# quilt setup [-d path-prefix] [-v] [--sourcedir dir] [--fuzz=N] [--spec-filter FILTER] [--slow|--fast] {specfile|seriesfile}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a setup -d 'Initializes a source tree from an rpm spec or a quilt series' -f

# quilt snapshot [-d]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a snapshot -d 'Take a snapshot of the working state' -f

# quilt top
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a top -d 'Name of topmost patch in the stack' -f

# quilt unapplied [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a unapplied -d 'List unapplied patches in the series' -f

# quilt upgrade
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a upgrade -d 'Upgrade meta-data from older version of quilt' -f
