# completions for quilt version 0.69

# List all patches in series, from latest to oldest (requires '-k').
function __fish_quilt_print_series
    set -l series (quilt series 2> /dev/null)
    printf '%s\n' $series[-1..1]
end

# Suggest '$token.patch' and '$token.diff' as names for new patches.
# An optional prefix can be passed to be removed from the patch name,
# e.g `__fish_quilt_complete_patch_name -p '-z'` for `quilt refresh -z[new_name]`.
function __fish_quilt_complete_patch_name -a prefix
    argparse --move-unknown 'p/prefix=&' -- $argv
    or return 1

    set -l name (commandline -ct)
    if test -z $name -o $name = "$_flag_prefix"
        return 1
    end

    printf '%s\n' $name
    set -l suggestions
    # output successful replace before other suggestions, requires '-k' to work
    if not string replace --filter --regex '\.(d(i(f(f)?)?)?)?$' '.diff' -- $name
        set -a suggestions $name.diff
    end
    if not string replace --filter --regex '\.(p(a(t(c(h)?)?)?)?)?$' '.patch' -- $name
        set -a suggestions $name.patch
    end
    printf '%s\n' $suggestions
    return 0
end

# For `quilt refresh -z[new_name]`, which looks like a short option
# but needs an argument connected to the switch.
function __fish_quilt_current_zarg -a quiet
    string match $quiet --regex '^-\w*z' -- (commandline -ct)
end

# Suggest more digits in number. Most useful for diff context and
# other options with a low number of digits.
function __fish_quilt_complete_integer
    set token (commandline -ct)
    if string match --quiet --regex '^\d*$' -- $token
        printf '%s\n' $token $token(seq 0 9)
    end
end

# Forward completions of 'quilt grep ...' to grep itself.
function __fish_quilt_complete_grep
    set -l command (commandline -px)
    while test (count $command) -gt 0 -a $command[1] != grep
        set -e command[1]
    end

    set -l arguments $command[2..]
    # quilt captures the initial -h arguments
    set -l quilt_help -h
    while test "$arguments[1]" = $quilt_help
        set -e arguments[1]
    end
    # and the first -- to stop capturing -h
    set -l quilt_sentinel --
    if test "$arguments[1]" = $quilt_sentinel
        set -e arguments[1]
    end

    complete -C "grep $(string escape -- $arguments)"
end

# List options for 'quilt setup --spec-filter ...'.
function __fish_quilt_complete_spec_filters
    # the current $QUILTRC can be safely ignored, since $QUILT_DIR is read before that
    set -l quilt_dir (quilt --quiltrc (echo 'printf "%s\n" "${QUILT_DIR}"' | psub) top -h 2>/dev/null | head -n 1)

    printf '%s\n' (path basename $quilt_dir/spec-filters/*)\t'Predefined Filter'
    printf '%s\n' (__fish_complete_path (commandline -ct))\t'Custom Filter'
end

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
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l quiltrc -d 'Config file to use' -xka "(__fish_complete_suffix --description=Config .rc quiltrc) -\t'No config'"
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l trace -d 'Trace internal shell commands (bash -x)' -fa verbose

# quilt --version
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -l version -d 'Show version' -f

# quilt add [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a add -d 'Add files to patch' -f
complete -c quilt -n '__fish_seen_subcommand_from add' -s P -d 'Patch to add files to' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from add' -rF

# quilt annotate [-P patch] {file}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a annotate -d 'Show which patches modify which lines in file' -f
complete -c quilt -n '__fish_seen_subcommand_from annotate' -s P -d 'Stop checking for changes at the specified patch' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from annotate' -rF

# quilt applied [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a applied -d 'List applied patches in the stack' -f
complete -c quilt -n '__fish_seen_subcommand_from applied' -d 'Up to and including' -fka '(__fish_quilt_print_series)'

# quilt delete [-r] [--backup] [patch|-n]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a delete -d 'Remove patch from the series' -f
complete -c quilt -n '__fish_seen_subcommand_from delete' -s r -d 'Remove patch file from the patches directory as well' -f
complete -c quilt -n '__fish_seen_subcommand_from delete' -l backup -d 'Rename the patch file to patch~' -f
complete -c quilt -n '__fish_seen_subcommand_from delete' -s n -d 'Delete the next patch after topmost' -f
complete -c quilt -n '__fish_seen_subcommand_from delete' -n 'not __fish_seen_argument -s n' -d 'Patch to remove' -fka '(__fish_quilt_print_series)'

# quilt diff [-p n|-p ab] [-u|-U num|-c|-C num] [--combine patch|-z] [-R] [-P patch] [--snapshot] [--diff=utility] [--no-timestamps] [--no-index] [--sort] [--color[=always|auto|never]] [file ...]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a diff -d 'Diff files in patch' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -s p -d 'File name style in patch' -xa '0\t"-p0 patch" 1\t"-p1 patch" ab\t"-p1 patch with a/file b/file"'
complete -c quilt -n '__fish_seen_subcommand_from diff' -s u -d 'Unified diff (3 lines of context)' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -s U -d 'Unified diff (N lines of context)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from diff' -s c -d 'Context diff (3 lines of context)' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -s C -d 'Context diff (N lines of context)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from diff' -l combine -d 'Combined diff between this patch and -P' -xka "(__fish_quilt_print_series)\t'First patch to combine'"
complete -c quilt -n '__fish_seen_subcommand_from diff' -s z -d 'Write to stdout' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -s R -d 'Reverse diff' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -s P -d 'Patch to diff' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from diff' -l snapshot -d 'Diff against snapshot' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -l diff -d 'Custom diff command' -xa '(__fish_complete_command)'
complete -c quilt -n '__fish_seen_subcommand_from diff' -l no-timestamps -d 'Do not include timestamps in headers' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -l no-index -d "Do not include 'Index: lines' in headers" -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -l sort -d 'Sort files by name' -f
complete -c quilt -n '__fish_seen_subcommand_from diff' -l color -d 'Use syntax coloring' -fa 'always auto never'
complete -c quilt -n '__fish_seen_subcommand_from diff' -F

# quilt edit file ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a edit -d 'Edit files in $EDITOR' -f
complete -c quilt -n '__fish_seen_subcommand_from edit' -rF

# quilt files [-v] [-a] [-l] [--combine patch] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a files -d 'Lists files changed by patch' -f
complete -c quilt -n '__fish_seen_subcommand_from files' -s v -d 'Pretty output' -f
complete -c quilt -n '__fish_seen_subcommand_from files' -s a -d 'List all files in all applied patches' -f
complete -c quilt -n '__fish_seen_subcommand_from files' -s l -d 'Add patch name to output' -f
complete -c quilt -n '__fish_seen_subcommand_from files' -l combine -d 'Combined listing between this patch and -P' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from files' -d 'List files changed by patch' -fka '(__fish_quilt_print_series)'

# quilt fold [-R] [-q] [-f] [-p strip-level]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a fold -d 'Integrate patch from stdin into the topmost patch' -f
complete -c quilt -n '__fish_seen_subcommand_from fold' -s R -d 'Apply patch in reverse' -f
complete -c quilt -n '__fish_seen_subcommand_from fold' -s q -d 'Quiet operation' -f
complete -c quilt -n '__fish_seen_subcommand_from fold' -s f -d 'Force apply, even if the patch has rejects' -f
complete -c quilt -n '__fish_seen_subcommand_from fold' -s p -d 'Number of components to strip from paths' -xa '(__fish_quilt_complete_integer)'

# quilt fork [new_name]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a fork -d 'Fork the topmost patch' -f
complete -c quilt -n '__fish_seen_subcommand_from fork' -fka '(__fish_quilt_complete_patch_name)'

# quilt graph [--all] [--reduce] [--lines[=num]] [--edge-labels=files] [-T ps] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a graph -d 'Visualize patch dependencies using dot' -f
complete -c quilt -n '__fish_seen_subcommand_from graph' -l all -d 'Include all applied patches and their dependencies' -f
complete -c quilt -n '__fish_seen_subcommand_from graph' -l reduce -d 'Eliminate transitive edges from the graph' -f
complete -c quilt -n '__fish_seen_subcommand_from graph' -l lines -d 'Compute dependencies by the lines the patches modify' -fa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from graph' -l edge-labels -d 'Label graph edges with the file names' -xa files
complete -c quilt -n '__fish_seen_subcommand_from graph' -s T -d 'Directly produce a PostScript output file' -xa ps
complete -c quilt -n '__fish_seen_subcommand_from graph' -n 'not __fish_seen_argument -l all' -d 'Patch to graph dependencies' -fka '(__fish_quilt_print_series)'

# quilt grep [-h|options] {pattern}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a grep -d 'Search through source files' -f
complete -c quilt -n '__fish_seen_subcommand_from grep' -fa '(__fish_quilt_complete_grep)'

# quilt header [-a|-r|-e] [--backup] [--strip-diffstat] [--strip-trailing-whitespace] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a header -d 'Print/change the header of patch' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -n 'not __fish_seen_argument -s a -s r -s e' -s a -d 'Append to patch header' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -n 'not __fish_seen_argument -s a -s r -s e' -s r -d 'Replace patch header' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -n 'not __fish_seen_argument -s a -s r -s e' -s e -d 'Edit header in $EDITOR' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -l backup -d 'Rename old version of a patch as patch~' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -l strip-diffstat -d 'Remove diffstat from the header' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -l strip-trailing-whitespace -d 'Strip whitespace at the end of lines' -f
complete -c quilt -n '__fish_seen_subcommand_from header' -d 'Print/change header of patch' -fka '(__fish_quilt_print_series)'

# quilt import [-p num] [-R] [-P patch] [-f] [-d {o|a|n}] patchfile ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a import -d 'Import external patches' -f
complete -c quilt -n '__fish_seen_subcommand_from import' -s p -d 'Number of directories to strip' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from import' -s R -d 'Apply patch in reverse' -f
complete -c quilt -n '__fish_seen_subcommand_from import' -s P -d 'Patch filename to use inside quilt' -xka '(__fish_quilt_complete_patch_name)'
complete -c quilt -n '__fish_seen_subcommand_from import' -s f -d 'Overwrite/update existing patches' -f
complete -c quilt -n '__fish_seen_subcommand_from import' -s d -d 'Which header to keep on overwrite' -xa "o\t'Keep old header' a\t'Keep all headers' n\t'Keep new header'"
complete -c quilt -n '__fish_seen_subcommand_from import' -d 'Patch to import' -fa '(__fish_complete_suffix .patch .diff)'

# quilt mail {--mbox file|--send} [-m text] [-M file] [--prefix prefix] [--sender ...] [--from ...] [--to ...] [--cc ...] [--bcc ...] [--subject ...] [--reply-to message] [--charset ...] [--signature file] [first_patch [last_patch]]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a mail -d 'Create or send mail messages from patches' -f
complete -c quilt -n '__fish_seen_subcommand_from mail' -l mbox -d 'Store the messages in an MBox file' -rF
complete -c quilt -n '__fish_seen_subcommand_from mail' -l send -d 'Send the messages directly' -f
complete -c quilt -n '__fish_seen_subcommand_from mail' -s m -d 'Text to use in the introduction' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -s M -d 'Read the introduction from file' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l prefix -d 'Alternate bracketed prefix for the subject' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l sender -d 'The envelope sender address to use' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l from -d 'Value for the From header' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l to -d 'Append recipient in the To header' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l cc -d 'Append recipient in the Cc header' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l bcc -d 'Append recipient in the Bcc header' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l subject -d 'Value for the From header' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l reply-to -d 'Add headers to reply to the message' -x
complete -c quilt -n '__fish_seen_subcommand_from mail' -l charset -d 'Specify a message encoding' -xa '(__fish_print_encodings)'
complete -c quilt -n '__fish_seen_subcommand_from mail' -l signature -d 'Append the signature to message' -rF
complete -c quilt -n '__fish_seen_subcommand_from mail' -n 'not __fish_seen_argument -l mbox -l send' -a -mbox -d 'Store the messages in an MBox file' -f
complete -c quilt -n '__fish_seen_subcommand_from mail' -n 'not __fish_seen_argument -l mbox -l send' -a -send -d 'Send the messages directly' -f
complete -c quilt -n '__fish_seen_subcommand_from mail' -n 'test (__fish_number_of_cmd_args_wo_opts) -eq 2' -d 'First patch to include' -fka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from mail' -n 'test (__fish_number_of_cmd_args_wo_opts) -eq 3' -d 'Last patch to include' -fka '(__fish_quilt_print_series)'

# quilt new [-p n] {patchname}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a new -d 'Start a new topmost patch' -f
complete -c quilt -n '__fish_seen_subcommand_from new' -s p -d 'File name style in patch' -xa "0\t'-p0 patch' 1\t'-p1 patch'"
complete -c quilt -n '__fish_seen_subcommand_from new' -xka '(__fish_quilt_complete_patch_name)'

# quilt next [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a next -d 'Name of patch after the topmost/given patch' -f
complete -c quilt -n '__fish_seen_subcommand_from next' -xka '(__fish_quilt_print_series)'

# quilt patches [-v] [--color[=always|auto|never]] {file} [files...]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a patches -d 'List patches that modify the file(s)' -f
complete -c quilt -n '__fish_seen_subcommand_from patches' -s v -d 'Pretty output' -f
complete -c quilt -n '__fish_seen_subcommand_from patches' -l color -d 'Use syntax coloring' -fa 'always auto never'
complete -c quilt -n '__fish_seen_subcommand_from patches' -rF

# quilt pop [-afRqv] [--refresh] [num|patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a pop -d 'Unapply patches from the stack' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -s a -d 'Remove all applied patches' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -s f -d 'Force remove and restore state from backup files' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -s R -d 'Always verify if the patch removes cleanly' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -s q -d 'Quiet operation' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -s v -d 'Verbose operation' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -l refresh -d 'Refresh every patch before it gets unapplied' -f
complete -c quilt -n '__fish_seen_subcommand_from pop' -n 'not __fish_seen_argument -s a' -d 'Pop N patches' -fka '(seq 1 (count (__fish_quilt_print_series)))'
complete -c quilt -n '__fish_seen_subcommand_from pop' -n 'not __fish_seen_argument -s a' -d 'Pop down to' -fka '(__fish_quilt_print_series)'

# quilt previous [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a previous -d 'Name of patch before the topmost/given patch' -f
complete -c quilt -n '__fish_seen_subcommand_from previous' -d 'Target patch' -fka '(__fish_quilt_print_series)'

# quilt push [-afqvm] [--fuzz=N] [--merge[=merge|diff3]] [--leave-rejects] [--color[=always|auto|never]] [--refresh] [num|patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a push -d 'Apply patches from the series' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -s a -d 'Apply all patches in the series file' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -s f -d 'Force apply, even if the patch has rejects' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -s q -d 'Quiet operation' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -s v -d 'Verbose operation' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -s m -d 'Merge the patch file into the original files' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -l merge -d 'Merge the patch file into the original files' -fa 'merge diff3'
complete -c quilt -n '__fish_seen_subcommand_from push' -l fuzz -d 'Maximum fuzz factor (in lines)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from push' -l leave-rejects -d 'Leave around the reject files patch produced' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -l color -d 'Use syntax coloring' -fa 'always auto never'
complete -c quilt -n '__fish_seen_subcommand_from push' -l refresh -d 'Automatically refresh every patch after it gets applied' -f
complete -c quilt -n '__fish_seen_subcommand_from push' -n 'not __fish_seen_argument -s a' -d 'Apply N patches' -fka '(seq 1 (count (__fish_quilt_print_series)))'
complete -c quilt -n '__fish_seen_subcommand_from push' -n 'not __fish_seen_argument -s a' -d 'Apply up to' -fka '(__fish_quilt_print_series)'

# quilt refresh [-p n|-p ab] [-u|-U num|-c|-C num] [-z[new_name]] [-f] [--no-timestamps] [--no-index] [--diffstat] [--sort] [--backup] [--strip-trailing-whitespace] [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a refresh -d 'Update patch with changes from source' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s p -d 'File name style in patch' -xa "0\t'-p0 patch' 1\t'-p1 patch' ab\t'-p1 patch with a/file b/file'"
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s u -d 'Unified diff (3 lines of context)' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s U -d 'Unified diff (N lines of context)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s c -d 'Context diff (3 lines of context)' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s C -d 'Context diff (N lines of context)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s z -d 'Create a new patch with the changes' -x
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n '__fish_quilt_current_zarg -q' -d 'Name of the new patch' -xka "(__fish_quilt_complete_patch_name -p (__fish_quilt_current_zarg))"
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n '__fish_quilt_current_zarg -q' -s h -e
complete -c quilt -n '__fish_seen_subcommand_from refresh' -n 'not __fish_quilt_current_zarg -q' -s f -d 'Refresh a patch that is not on top' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l no-timestamps -d 'Do not include timestamps in header' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l no-index -d "Do not include 'Index: lines' in header" -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l diffstat -d 'Add or replace diffstat section to header' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l sort -d 'Sort files by name' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l backup -d 'Rename old patch file to patch~' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -l strip-trailing-whitespace -d 'Strip whitespace at the end of lines' -f
complete -c quilt -n '__fish_seen_subcommand_from refresh' -d 'Patch to refresh' -fka '(__fish_quilt_print_series)'

# quilt remove [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a remove -d 'Remove files from the patch' -f
complete -c quilt -n '__fish_seen_subcommand_from remove' -s P -d 'Remove files from named patch' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from remove' -rF

# quilt rename [-P patch] new_name
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a rename -d 'Rename the topmost/given patch' -f
complete -c quilt -n '__fish_seen_subcommand_from rename' -s P -d 'Patch to rename' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from rename' -xka "(__fish_quilt_complete_patch_name)\t'Suggestion'"
complete -c quilt -n '__fish_seen_subcommand_from rename' -xka "(__fish_quilt_print_series)\t'Old names'"

# quilt revert [-P patch] {file} ...
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a revert -d 'Revert file changes in patch' -f
complete -c quilt -n '__fish_seen_subcommand_from revert' -s P -d 'Revert changes in patch' -xka '(__fish_quilt_print_series)'
complete -c quilt -n '__fish_seen_subcommand_from revert' -rF

# quilt series [--color[=always|auto|never]] [-v]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a series -d 'List all patches in the series' -f
complete -c quilt -n '__fish_seen_subcommand_from series' -l color -d 'Use syntax coloring' -fa 'always auto never'
complete -c quilt -n '__fish_seen_subcommand_from series' -s v -d 'Pretty output' -f

# quilt setup [-d path-prefix] [-v] [--sourcedir dir] [--fuzz=N] [--spec-filter FILTER] [--slow|--fast] {specfile|seriesfile}
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a setup -d 'Initializes a source tree from an rpm spec or a quilt series' -f
complete -c quilt -n '__fish_seen_subcommand_from setup' -s d -d 'Path prefix for the resulting source tree' -x
complete -c quilt -n '__fish_seen_subcommand_from setup' -s v -d 'Verbose debug output' -f
complete -c quilt -n '__fish_seen_subcommand_from setup' -l sourcedir -d 'Directory for package sources' -xa '(__fish_complete_directories)'
complete -c quilt -n '__fish_seen_subcommand_from setup' -l fuzz -d 'Maximum fuzz factor (in lines)' -xa '(__fish_quilt_complete_integer)'
complete -c quilt -n '__fish_seen_subcommand_from setup' -l spec-filter -d 'Apply a filter before passing the spec file to rpmbuild' -xa '(__fish_quilt_complete_spec_filters)'
complete -c quilt -n '__fish_seen_subcommand_from setup' -n 'not __fish_seen_argument -l slow -l fast' -l slow -d 'Use original method to process the spec file' -f
complete -c quilt -n '__fish_seen_subcommand_from setup' -n 'not __fish_seen_argument -l slow -l fast' -l fast -d 'Use new method to process the spec file' -f
complete -c quilt -n '__fish_seen_subcommand_from setup' -xa '(__fish_complete_suffix .spec series)'

# quilt snapshot [-d]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a snapshot -d 'Take a snapshot of the working state' -f
complete -c quilt -n '__fish_seen_subcommand_from snapshot' -s d -d 'Only remove current snapshot' -f

# quilt top
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a top -d 'Name of topmost patch in the stack' -f

# quilt unapplied [patch]
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a unapplied -d 'List unapplied patches in the series' -f
complete -c quilt -n '__fish_seen_subcommand_from unapplied' -d 'Patch to assume as top' -fka '(__fish_quilt_print_series)'

# quilt upgrade
complete -c quilt -n "not __fish_seen_subcommand_from $commands" -a upgrade -d 'Upgrade meta-data from older version of quilt' -f
