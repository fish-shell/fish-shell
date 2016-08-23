# Fish completions for Perforce (p4) SCM
# Based on the list of commands from
# https://www.perforce.com/perforce/r16.1/manuals/cmdref/
# Author: Boris Aranovich https://github.com/nomaed

#########################################################
### p4 command parsing
#########################################################

function __fish_print_p4_client_name -d "Use `p4 info` output to query for current client name"
    set -l p4info (p4 info -s 2> /dev/null)
    if string match -qr '^Client unknown' $p4info
        return
    end
    string match -r '^Client name: .+' $p4info | string replace 'Client name: ' ''
end

#########################################################

function __fish_print_p4_user_name -d "Use `p4 info` output to query for current client name"
    string match -r '^User name: .+' (p4 info 2> /dev/null) | string replace 'User name: ' ''
end

#########################################################

function __fish_print_p4_changelists -d "Reformat output from `p4 changes` to simple format. Specify 'detailed!' as first argument to use username@workspace prefix"
    set -l detailed
    if test -n "$argv"
        and test $argv[1] = "detailed!"
        set detailed true
        set -e argv[1]
    end

    # The format of `p4 changes -L` is as follows, for each changelist:
    # Change 1234 on YYYY/MM/DD by user@workspace *status*\n
    # \n
    #   \t  Description text line\n
    #   \t  Description text another line\n
    # \n
    set -l changes (p4 changes -L $argv)

    set -l result
    for line in (string trim -- $changes)
        if test -z "$line"
            continue
        end
        # see output format ^^^
        set -l change_match (string match -ar '^Change ([0-9]+) on [0-9/]+ by (\S+).*$' $line)
        if test -n "$change_match"
            if test -n "$result"
                echo $result
                set result
            end
            set result $change_match[2]\t
            if test -n "$detailed"
                set result $result $change_match[3]:
            end
        else
            set result $result $line
        end
    end

    if test -n "$result"
        echo $result
    end
end

#########################################################

function __fish_print_p4_opened_files -d "Use `p4 diff` to output the names of all opened files"
    # p4 opened -s | sed 's/#.*//' | p4 -x - where | awk '/^\// {print $3}'
    # p4 opened -s | string replace -ar '(^\S+).*$' '$1' | p4 -x - where | string replace -ar '\S+\s\S+\s(\S+)' '$1'
    string replace -a "$PWD/" '' (p4 diff -sa -sb -sr)
end

#########################################################

function __fish_print_p4_branches -d "Prints the list of all defined branches on the server"
    set -l branches (p4 branches)
    for branch in $branches
        # "Branch branch-name YYYY/MM/DD 'description text'"
        set -l matches (string match -ar '^Branch\s+(\S+)[^\']+\'(.+)\'$' $branch)
        if not set -q matches[2]
            # skip $branch if no match for branch name with description
            continue
        end
        # matches[2] = branch name; matches[3] = description
        echo -n $matches[2]
        if not set -q matches[3]
            echo -n \t$matches[3]
        end
        echo
    end
end

#########################################################

function __fish_print_p4_streams
    # I do not have an example of p4 streams output
    #p4 streams 2> /dev/null
end

#########################################################

function __fish_print_p4_users -d "Lists perforce users suitable for list of completions"
    # `p4 users` output format:
    #   "username <email@address> (Full Name) accessed YYYY/MM/DD"
    # function will output it as:
    #   username[TAB]Full Name <email@address>
    string replace -ar '(^\S+) <([^>]+)> \(([^\)]+)\).*$' '$1'\t'$3 <$2>' (p4 users)
end

#########################################################

function __fish_print_p4_workspaces -d "Lists current user's workspaces"
    set -l user (__fish_print_p4_user_name)
    if test -z "$user"
        return
    end
    # "Client clientname YYYY/MM/DD root /home/user/workspace/path 'description text'"
    string replace -ar '^Client (\S+) \S+ root (\S+) \'(.+)\'$' '$1'\t'$3' (p4 clients -u $user)
end

#########################################################

function __fish_print_p4_workspace_changelists -d "Lists all changelists for current user"
    set -l client (__fish_print_p4_client_name)
    if test -n "$client"
        __fish_print_p4_changelists -c $client $argv
    end
end

#########################################################

function __fish_print_p4_pending_changelists -d "Lists all *pending* changelists. If 'default' argument is provided, default changelist will also be listed"
    if set -q argv[1]
        and test $argv[1] = "default"
        echo default\tDefault changelist
    end
    __fish_print_p4_workspace_changelists -s pending
end

#########################################################

function __fish_print_p4_shelved_changelists -d "Lists all changelists with *shelved* files"
    __fish_print_p4_workspace_changelists -s shelved
end

#########################################################
### completion helpers
#########################################################

function __fish_print_p4_commands_list -d "Lists p4 commands"
    set -l commands add annotate attribute branch branches change changes changelist changelists clean client clients copy counter counters cstat delete depot depots describe diff diff2 dirs edit filelog files fix fixes flush fstat grep group groups have help info integ integrate integrated interchanges istat job jobs key keys label labels labelsync list lock logger login logout merge move opened passwd populate print protect protects prune rec reconcile rename reopen resolve resolved revert review reviews set shelve status sizes stream streams submit sync tag tickets unlock unshelve update user users where workspace workspaces
    for i in $commands
        echo $i
    end
end

#########################################################

function __fish_print_p4_env_vars -d "Lists environment variables that can be consumed by perforce"
    set -l env_vars P4CLIENT P4PORT P4PASSWD P4USER P4CONFIG P4DIFF P4EDITOR P4MERGE P4CHARSET P4TRUST P4PAGER PWD TMP TEMP P4TICKETS P4LANGUAGE P4LOGINSSO P4COMMANDCHARSET P4DIFFUNICODE P4MERGEUNICODE P4CLIENTPATH P4AUDIT P4JOURNAL P4LOG P4PORT P4ROOT P4DEBUG P4NAME P4SSLDIR
    for var in $env_vars
        echo $var
    end
end

#########################################################

function __fish_print_p4_file_types -d "Lists all of available file modes"
    set -l base_types text binary symlink apple resource unicode utf16
    for type in $base_types
        printf '%s\t%s\n' $type+m 'always set modtime' $type+w 'always writeable' $type+x 'exec bit set' $type+k '$Keyword$ expansion of Id, Header, Author, Date, DateUTC, DateTime, DateTimeUTC, DateTimeTZ, Change, File, Revision' $type+ko '$Keyword$ expansion of Id, Header only' $type+l 'exclusive open: disallow multiple opens' $type+C 'server stores compressed file per revision' $type+D 'server stores deltas in RCS format' $type+F 'server stores full file per revision' $type+S 'server stores only single head rev., or specify number to <n> of revisions' $type+X 'server runs archive trigger to access files'
    end
end

#########################################################

function __fish_print_p4_help_keywords -d "Keywords for `p4 help`"
    echo administration\t"Help on specialized administration topics"
    echo charset\t"Describes how to control Unicode translation"
    echo commands\t"Lists all the Perforce commands"
    echo configurables\t"Describes all of the server configuration variables"
    echo dvcs\t"Describes decentralized version control with Perforce"
    echo environment\t"Lists the Perforce environment variables and their meanings"
    echo filetypes\t"Lists the Perforce filetypes and their meanings"
    echo jobview\t"Describes Perforce jobviews"
    echo legal\t"Legal and license information"
    echo networkaddress\t"Help on network address syntax"
    echo replication\t"Describes specialized replication topics"
    echo revisions\t"Describes Perforce revision specifiers"
    echo simple\t"Provides short descriptions of the eight most basic Perforce commands"
    echo usage\t"Lists the six options available with all Perforce commands"
    echo views\t"Describes the meaning of Perforce views"
end

#########################################################

function __fish_print_p4_diff_options -d "Options for `p4 diff -d`"
    echo n\t"RCS output format, showing additions and deletions made to the file and associated line ranges"
    echo c\t"context output format, showing line number ranges and N lines of context around the changes" #
    echo s\t"summary output format, showing only the number of chunks and lines added, deleted, or changed"
    echo u\t"unified output format, showing added and deleted lines with N lines of context, in a form compatible with the patch(1) utility" #
    echo l\t"ignore line-ending (CR/LF) convention when finding diffs"
    echo b\t"ignore changes made within whitespace; this option implies -dl"
    echo w\t"ignore whitespace altogether; this option implies -dl"
end

#########################################################

function __fish_print_p4_resolve_options -d "Options for `p4 merge` using -a, -A and -d"
    switch "$argv[1]"
        case a
            echo m\t"Automatic: accept the Perforce-recommended file revision (yours, their, merge)"
            echo y\t"Accept Yours, ignore theirs"
            echo t\t"Accept Theirs"
            echo s\t"Safe Accept: merge if no conflict, ignore file otherwise"
            echo f\t"Force Accept: conflicted changes will leave conflict markers in the file"
        case A
            echo b\t"Resolve attributes set by p4 attribute"
            echo b\t"Resolve file branching; that is, integrations where the source is edited and the target is deleted"
            echo c\t"Resolve file content changes"
            echo d\t"Integrations where the source is deleted and target is deleted"
            echo t\t"Filetype changes"
            echo m\t"Move and renames"
        case d
            echo b\t"Ignore whitespace-only changes (for instance, a tab replaced by eight spaces)"
            echo w\t"Ignore whitespace altogether (for instance, deletion of tabs or other whitespace)"
            echo l\t"Ignore differences in line-ending convention"
    end
end

#########################################################

function __fish_print_p4_parallel_options -d "Values for --parallel option in various commands"
    set -l mode
    if test -n "$argv"
        set mode $argv[1]
    end

    # for now only looks that mode is set, later it will need to have a specific setting
    echo 'threads='\t"sends files concurrently using N independent network connections"
    echo 'batch='\t"specifies the number of files in a batch"
    test "$mode"
    or echo 'batchsize='\t"specifies the number of bytes in a batch"
    echo 'min='\t"specifies the minimum number of files in a parallel sync"
    test "$mode"
    or echo 'minsize='\t"specifies the minimum number of bytes in a parallel sync"
end

#########################################################

function __fish_print_p4_submit_options -d "Options for `p4 submit`"
    echo 'submitunchanged'\t"Submit all open files (default behavior)"
    echo 'submitunchanged+reopen'\t"Submit all open files + reopen to default changelist"
    echo 'revertunchanged'\t"Revert unmodified files, submit all the rest"
    echo 'revertunchanged+reopen'\t"Revert unmodified files, submit all the rest + reopen them in default changelist"
    echo 'leaveunchanged'\t"Move unchanged files to default changelist, submit all the rest"
    echo 'leaveunchanged+reopen'\t"Submit only modified files + reopen all (modified and unmodified) in default changelist"
end

#########################################################

function __fish_print_p4_noretransfer_options -d "Options for `p4 submit --noretransfer`"
    echo 1\t"server avoids re-transferring files that have already been archived after a failed submit operation"
    echo 0\t"server re-transfers all files after a failed submit operation"
end

#########################################################

function __fish_print_p4_integrate_output_options -d "Options for `p4 resolve -O`"
    echo b\t"outputs the base revision for the merge (if any)"
    echo r\t"outputs the resolves that are being scheduled"
end

#########################################################

function __fish_print_p4_integrate_resolve_options -d "Options for `p4 resolve -R`"
    echo b\t"schedules a branch resolve instead of branching the target files automatically"
    echo d\t"schedules a delete resolve instead of deleting the target files automatically"
    echo s\t"skips cherry-picked revisions that have already been integrated"
end

#########################################################

function __fish_p4_not_in_command -d "Checks that prompt is not inside of p4 command"
    for i in (commandline -opc)
        if contains -- $i (__fish_print_p4_commands_list)
            return 1
        end
    end
    return 0
end

#########################################################

# Perforce command is a single word that comes either as first argument
# or directly after global options.
# To test whether we're in command, it's enough that a command will appear
# in the arguments, even though if more than a single command is specified,
# p4 will complain.
function __fish_p4_is_using_command -d "Checks if prompt is in a specific command"
    if contains -- $argv[1] (commandline -opc)
        return 0
    end
    return 1
end

#########################################################

function __fish_p4_register_command -d "Adds a completion for a specific command"
    complete -c p4 -n "__fish_p4_not_in_command" -a $argv[1] $argv[2..-1]
end

#########################################################

function __fish_p4_register_command_option -d "Adds a specific option for a command"
    complete -c p4 -n "__fish_p4_is_using_command $argv[1]" $argv[2..-1]
end

#########################################################
### global options -- can be used before any command
#########################################################

complete -c p4 -n __fish_p4_not_in_command -s b -x -d 'Specifies a batch size (number of arguments) to use when processing a command from a file with the -x argfile option'
complete -c p4 -n __fish_p4_not_in_command -s c -x -d 'Overrides any P4CLIENT setting with the specified client name'
complete -c p4 -n __fish_p4_not_in_command -s d -r -d 'Overrides any PWD setting (current working directory) and replaces it with the specified directory'
complete -c p4 -n __fish_p4_not_in_command -s I -d 'Specify that progress indicators, if available, are desired. This option is not compatible with the -s and -G options'
complete -c p4 -n __fish_p4_not_in_command -s G -d 'Causes all output (and batch input for form commands with -i) to be formatted as marshalled Python dictionary objects'
complete -c p4 -n __fish_p4_not_in_command -s H -x -a '(__fish_print_hostnames)' -d 'Overrides any P4HOST setting and replaces it with the specified hostname'
complete -c p4 -n __fish_p4_not_in_command -s p -x -d 'Overrides any P4PORT setting with the specified protocol:host:port'
complete -c p4 -n __fish_p4_not_in_command -s P -x -d 'Overrides any P4PASSWD setting with the specified password.'
complete -c p4 -n __fish_p4_not_in_command -s r -x -d 'Specifies the number of times to retry a command (notably, p4 sync) if the network times out.'
complete -c p4 -n __fish_p4_not_in_command -s s -d 'Prepends a descriptive field (for example, text:, info:, error:, exit:) to each line of output produced by a Perforce command. This is most often used when scripting.'
complete -c p4 -n __fish_p4_not_in_command -s u -x -a '(__fish_print_p4_users)' -d 'Overrides any P4USER, USER, or USERNAME setting with the specified user name.'
complete -c p4 -n __fish_p4_not_in_command -s x -r -d 'Instructs Perforce to read arguments, one per line, from the specified file. If file is a single hyphen (-), then standard input is read.'
complete -c p4 -n __fish_p4_not_in_command -s C -x -a '(__fish_print_encodings)' -d 'Overrides any P4CHARSET setting with the specified character set.'
complete -c p4 -n __fish_p4_not_in_command -s Q -x -a '(__fish_print_encodings)' -d 'Overrides any P4COMMANDCHARSET setting with the specified character set.'
complete -c p4 -n __fish_p4_not_in_command -s L -x -d 'This feature is reserved for system integrators.'
complete -c p4 -n __fish_p4_not_in_command -s z -x -d 'Causes output of many reporting commands to be in the same tagged format as that generated by p4 fstat.'
complete -c p4 -n __fish_p4_not_in_command -s q -d 'Quiet mode; suppress all informational message and report only warnings or errors.'
complete -c p4 -n __fish_p4_not_in_command -s V -d 'Displays the version of the p4 application and exits.'
complete -c p4 -n __fish_p4_not_in_command -s h -d 'Displays basic usage information and exits.'

#########################################################
### sub-commands
#########################################################

__fish_p4_register_command add -d "Open a new file to add it to the depot"
__fish_p4_register_command annotate -d "Print file lines along with their revisions"
__fish_p4_register_command attribute -f -d "Set per-revision attributes on revisions"
__fish_p4_register_command branch -f -d "Create or edit a branch specification"
__fish_p4_register_command branches -f -d "Display list of branches"
__fish_p4_register_command change -f -d "Create or edit a changelist description"
__fish_p4_register_command changes -f -d "Display list of pending and submitted changelists"
__fish_p4_register_command changelist -f -d "Create or edit a changelist description"
__fish_p4_register_command changelists -f -d "Display list of pending and submitted changelists"
__fish_p4_register_command clean -d "Delete or refresh local files to match depot state"
__fish_p4_register_command client -f -d "Create or edit a client specification and its view"
__fish_p4_register_command clients -f -d "Display list of known clients"
__fish_p4_register_command copy -d "Schedule copy of latest rev from one file to another"
__fish_p4_register_command counter -f -d "Display, set, or delete a counter"
__fish_p4_register_command counters -f -d "Display list of known counters"
__fish_p4_register_command cstat -f -d "Dump change/sync status for current client"
__fish_p4_register_command delete -d "Open an existing file to delete it from the depot"
__fish_p4_register_command depot -f -d "Create or edit a depot specification"
__fish_p4_register_command depots -f -d "Display list of depots"
__fish_p4_register_command describe -x -d "Display a changelist description"
__fish_p4_register_command diff -d "Display diff of client file with depot file"
__fish_p4_register_command diff2 -d "Display diff of two depot files"
__fish_p4_register_command dirs -f -d "List subdirectories of a given depot directory"
__fish_p4_register_command edit -d "Open an existing file for edit"
__fish_p4_register_command filelog -d "List revision history of files"
__fish_p4_register_command files -d "List files in the depot"
__fish_p4_register_command fix -f -d "Mark jobs as being fixed by named changelists"
__fish_p4_register_command fixes -f -d "List what changelists fix what job"
__fish_p4_register_command flush -d "Fake a 'p4 sync' by not moving files"
__fish_p4_register_command fstat -d "Dump file info"
__fish_p4_register_command grep -d "Print lines from text files matching a pattern"
__fish_p4_register_command group -f -d "Change members of a user group"
__fish_p4_register_command groups -f -d "List groups (of users)"
__fish_p4_register_command have -f -d "List revisions last synced"
__fish_p4_register_command help -f -d "Print the requested help message"
__fish_p4_register_command info -f -d "Print out client/server information"
__fish_p4_register_command integ -d "Schedule integration from one file to another"
__fish_p4_register_command integrate -d "Schedule integration from one file to another"
__fish_p4_register_command integrated -f -d "Show integrations that have been submitted"
__fish_p4_register_command interchanges -f -d "Report changes that have not yet been integrated"
__fish_p4_register_command istat -f -d "Show integrations needed for a stream"
__fish_p4_register_command job -f -d "Create or edit a job (defect) specification"
__fish_p4_register_command jobs -f -d "Display list of jobs"
__fish_p4_register_command key -f -d "Display, set, or delete a key/value pair"
__fish_p4_register_command keys -f -d "Display list of known keys and their values"
__fish_p4_register_command label -f -d "Create or edit a label specification and its view"
__fish_p4_register_command labels -f -d "Display list of labels"
__fish_p4_register_command labelsync -f -d "Synchronize label with the current client contents"
__fish_p4_register_command list -d "Create an in-memory (label) list of depot files"
__fish_p4_register_command lock -d "Lock an opened file against changelist submission"
__fish_p4_register_command logger -f -d "Report what jobs and changelists have changed"
__fish_p4_register_command login -f -d "Login to Perforce by obtaining a session ticket"
__fish_p4_register_command logout -f -d "Logout of Perforce by removing or invalidating a ticket"
__fish_p4_register_command merge -d "Schedule merge (integration) from one file to another"
__fish_p4_register_command move -d "Moves files from one location to another"
__fish_p4_register_command opened -d "Display list of files opened for pending changelist"
__fish_p4_register_command passwd -f -d "Set the user's password on the server (and Windows client)"
__fish_p4_register_command populate -d "Populate a branch or stream with files"
__fish_p4_register_command print -d "Retrieve a depot file to the standard output"
__fish_p4_register_command protect -f -d "Modify protections in the server namespace"
__fish_p4_register_command protects -f -d "Display protections in place for a given user/path"
__fish_p4_register_command prune -d "Remove unmodified branched files from a stream"
__fish_p4_register_command rec -f -d "Reconcile client to offline workspace changes"
__fish_p4_register_command reconcile -f -d "Reconcile client to offline workspace changes"
__fish_p4_register_command rename -d "Moves files from one location to another"
__fish_p4_register_command reopen -d "Change the type or changelist number of an opened file"
__fish_p4_register_command resolve -d "Merge open files with other revisions or files"
__fish_p4_register_command resolved -d "Show files that have been merged but not submitted"
__fish_p4_register_command revert -d "Discard changes from an opened file"
__fish_p4_register_command review -f -d "List and track changelists (for the review daemon)"
__fish_p4_register_command reviews -d "Show what users are subscribed to review files"
__fish_p4_register_command set -x -d "Set variables in the registry (Windows only)"
__fish_p4_register_command shelve -x -d "Store files from a pending changelist into the depot"
__fish_p4_register_command status -f -d "Preview reconcile of client to offline workspace changes"
__fish_p4_register_command sizes -d "Display size information for files in the depot"
__fish_p4_register_command stream -f -d "Create or edit a stream specification"
__fish_p4_register_command streams -f -d "Display list of streams"
__fish_p4_register_command submit -d "Submit open files to the depot"
__fish_p4_register_command sync -f -d "Synchronize the client with its view of the depot"
__fish_p4_register_command tag -d "Tag files with a label"
__fish_p4_register_command tickets -f -d "Display list of session tickets for this user"
__fish_p4_register_command unlock -d "Release a locked file but leave it open"
__fish_p4_register_command unshelve -d "Restore shelved files from a pending changelist"
__fish_p4_register_command update -f -d "Update the client with its view of the depot"
__fish_p4_register_command user -f -d "Create or edit a user specification"
__fish_p4_register_command users -f -d "Display list of known users"
__fish_p4_register_command where -d "Show how file names map through the client view"
__fish_p4_register_command workspace -f -d "Create or edit a client specification and its view"
__fish_p4_register_command workspaces -f -d "Display list of known clients"

#########################################################
### sub-command options
#########################################################

#-----------------------------------------------------
#--> Help
#-----------------------------------------------------

# help
__fish_p4_register_command_option help -f -a '(__fish_print_p4_help_keywords)'

# info
__fish_p4_register_command_option info -s s -d "Short output (removes information that requires DB search)"

#-----------------------------------------------------
#--> Client workspace
#-----------------------------------------------------

# clean
__fish_p4_register_command_option clean -a '(__fish_print_p4_opened_files)'
__fish_p4_register_command_option clean -s a -d "Added files: Find files in the workspace that have no corresponding files in the depot and delete them"
__fish_p4_register_command_option clean -s d -d "Deleted files: Find those files in the depot that do not exist in your workspace and add them to the workspace"
__fish_p4_register_command_option clean -s e -d "Edited files: Find files in the workspace that have been modified and restore them to the last file version that has synced from the depot"
__fish_p4_register_command_option clean -s I -d "Do not perform any ignore checking; ignore any settings specified by P4IGNORE for added files"
__fish_p4_register_command_option clean -s l -d "Display output in local file syntax with relative paths, similar to the workspace-centric view of p4 status"
__fish_p4_register_command_option clean -s n -d "Preview the results of the operation without performing any action"

# client, workspace @TODO: -Fs (only in -f), -c (only in -S stream)
for a in 'client' 'workspace'
    __fish_p4_register_command_option $a -x -a '(__fish_print_p4_workspaces)'
    __fish_p4_register_command_option $a -s f -d "Allows the last modification date, which is normally read-only, to be set"
    __fish_p4_register_command_option $a -s d -f -a '(__fish_print_p4_workspaces)' -d "Delete the specified client workspace whether or not the workspace is owned by the user"
    # __fish_p4_register_command_option $a -a '-Fs' -d 'Deletes client with shelves (must follow -f)'
    __fish_p4_register_command_option $a -s F -a 's' -d 'Deletes client with shelves (must follow -f)'
    __fish_p4_register_command_option $a -s o -d "Write the client workspace spec to standard output"
    __fish_p4_register_command_option $a -s i -d "Read the client workspace spec from standard input"
    __fish_p4_register_command_option $a -s c -x -a '(__fish_print_p4_workspace_changelists)' -d "When used with -S stream, displays the workspace spec that would have been created for a stream at the moment the change was submitted"
    __fish_p4_register_command_option $a -s s -d "Switch workspace view"
    __fish_p4_register_command_option $a -s t -x -d "Specify a client workspace Template"
    __fish_p4_register_command_option $a -l serverid -x -d "Forcefully delete workspace that is bound to another server"
    __fish_p4_register_command_option $a -s S -x -a '(__fish_print_p4_streams)' -d "Associates the workspace with the specified stream"
end

# clients, workspaces @TODO -U and others are mutually exclusive
for a in 'clients' 'workspaces'
    __fish_p4_register_command_option $a -s a -d "List all client workspaces, not just workspaces bound to this server"
    __fish_p4_register_command_option $a -s e -x -d "List only client workspaces matching filter (case-sensitive)"
    __fish_p4_register_command_option $a -s E -x -d "List only client workspaces matching filter (case-insensitive)"
    __fish_p4_register_command_option $a -s m -x -d "List only the first max client workspaces"
    __fish_p4_register_command_option $a -s s -x -d "List only client workspaces bound to the specified serverID. On an edge server, the -s option defaults to the edge server’s serverID"
    __fish_p4_register_command_option $a -s S -x -a '(__fish_print_p4_streams)' -d "List client workspaces associated with the specified stream"
    __fish_p4_register_command_option $a -s t -d "Display the time as well as the date of the last update to the workspace"
    __fish_p4_register_command_option $a -s u -x -a '(__fish_print_p4_users)' -d "List only client workspaces owned by user"
    __fish_p4_register_command_option $a -s U -d "List only client workspaces unloaded with p4 unload"
end

# flush @TODO
# have @TODO
# ignores @TODO

# sync @TODO --parallel has unique key-value pair syntax: --parallel=optq1=n,opt2=n,opt3=n,opt4=n
__fish_p4_register_command_option sync -s f -d "Force sync, overwrite all unopened files"
__fish_p4_register_command_option sync -s k -d "Keep existing workspace files, alias for p4 flush"
__fish_p4_register_command_option sync -s L -d "perform the sync on a list of valid file arguments in full depot syntax with a valid revision number"
__fish_p4_register_command_option sync -s m -x -d "Sync only the first max files specified"
__fish_p4_register_command_option sync -s n -d "Preview mode: Display the results of the sync without actually syncing"
__fish_p4_register_command_option sync -s N -d "Preview mode: Display network traffic estimates, don't actually sync"
__fish_p4_register_command_option sync -s p -d "Populate a client workspace, but do not update the have list"
__fish_p4_register_command_option sync -l parallel -x -a '(__fish_print_p4_parallel_options)' -d "Specify options for parallel file transfer"
__fish_p4_register_command_option sync -s q -d "Quiet operation: suppress normal output messages"
__fish_p4_register_command_option sync -s r -d "Reopen files that are mapped to new locations in the depot, in the new location"
__fish_p4_register_command_option sync -s s -d "Safe sync: Compare the content in your client workspace against what was last synced"

# update @TODO
# where @TODO

#-----------------------------------------------------
#--> Files
#-----------------------------------------------------

# add
__fish_p4_register_command_option add -s c -x -a '(__fish_print_p4_pending_changelists default)' -d "Changelist number"
__fish_p4_register_command_option add -s d -d "Revert and re-add"
__fish_p4_register_command_option add -s f -d "Use wildcard characters for files"
__fish_p4_register_command_option add -s I -d "Do not use P4IGNORE"
__fish_p4_register_command_option add -s n -d "Preview operation, don't change files"
__fish_p4_register_command_option add -s t -x -a '(__fish_print_p4_file_types)' -d "File type"

# attribute @TODO
# copy @TODO
# delete @TODO
# diff @TODO
# diff2 @TODO
# dirs @TODO

# edit
__fish_p4_register_command_option edit -s c -x -a '(__fish_print_p4_pending_changelists default)' -d "Changelist number"
__fish_p4_register_command_option edit -s k -d "Keep existing workspace files; mark the file as open for edit even if the file is not in the client view"
__fish_p4_register_command_option edit -s n -d "Preview operation, don't change files"
__fish_p4_register_command_option edit -s t -x -a '(__fish_print_p4_file_types)' -d "File type"

# files @TODO
# fstat @TODO
# grep @TODO
# move @TODO
# lock @TODO
# print @TODO
# reconcile @TODO
# rename @TODO
# revert @TODO
# status @TODO
# sizes @TODO
# unlock @TODO

#-----------------------------------------------------
#--> Changelists
#-----------------------------------------------------

# change, changelist
for a in 'change' 'changelist'
    __fish_p4_register_command_option $a -x -a '(__fish_print_p4_pending_changelists)'
    __fish_p4_register_command_option $a -s s -d "Allows jobs to be assigned arbitrary status values on submission of the changelist, rather than the default status of closed"
    __fish_p4_register_command_option $a -s f -d "Force operation (The -u and the -f options are mutually exclusive)"
    __fish_p4_register_command_option $a -s u -d "Update a submitted changelist (The -u and the -f options are mutually exclusive)"
    __fish_p4_register_command_option $a -s O -x -a '(__fish_print_p4_pending_changelists)' -d "Changelist number"
    __fish_p4_register_command_option $a -s d -x -a '(__fish_print_p4_pending_changelists)' -d "Delete a changelist"
    __fish_p4_register_command_option $a -s o -d "Writes the changelist spec to standard output"
    __fish_p4_register_command_option $a -s i -d "Read the changelist spec from standard input"
    __fish_p4_register_command_option $a -s t -x -a "restricted public" -d "Modifies the 'Type' of the change"
    __fish_p4_register_command_option $a -s U -x -a '(__fish_print_p4_users)' -d "Changes the 'User' of the change"
end

# changes, changelists
for a in 'changes' 'changelists'
    __fish_p4_register_command_option $a -s i -d "Include changelists that affected files that were integrated with the specified files"
    __fish_p4_register_command_option $a -s t -d "Display the time as well as the date of each change"
    __fish_p4_register_command_option $a -s l -d "List long output, with the full text of each changelist description"
    __fish_p4_register_command_option $a -s L -d "List long output, with the full text of each changelist description truncated at 250 characters"
    __fish_p4_register_command_option $a -s f -d "View restricted changes (requires admin permission)"
    __fish_p4_register_command_option $a -c c -x -a '(__fish_print_p4_workspace_changelists)' -d "List only changes made from the named client workspace"
    __fish_p4_register_command_option $a -c m -x -d "List only the highest numbered max changes"
    __fish_p4_register_command_option $a -c s -x -a 'pending submitted shelved' -d "Limit the list to the changelists with the given status"
    __fish_p4_register_command_option $a -c u -x -a '(__fish_print_p4_users)' -d "List only changes made from the named user"

end

# describe @TODO: -dc<n>, -du<n>
__fish_p4_register_command_option describe -x -a '(__fish_print_p4_workspace_changelists)'
__fish_p4_register_command_option describe -s f -d 'Force the display of descriptions for restricted changelists'
__fish_p4_register_command_option describe -s O -d 'Specify the original changelist number'
__fish_p4_register_command_option describe -s s -d 'Short output without diffs'
__fish_p4_register_command_option describe -s S -d 'Display shelved files with diffs'
__fish_p4_register_command_option describe -s d -x -a '(__fish_print_p4_diff_options)' -d 'Diff'

# filelog @TODO

# opened
__fish_p4_register_command_option opened -s a -d "List opened files in all client workspaces"
__fish_p4_register_command_option opened -s c -x -a '(__fish_print_p4_pending_changelists default)' -d "List the files in a pending changelist"
__fish_p4_register_command_option opened -s C -x -a '(__fish_print_p4_workspaces)' -d "List only files that are open in the specified client workspace"
__fish_p4_register_command_option opened -s m -x -d "List only the first max open files"
__fish_p4_register_command_option opened -s s -d "Short output; do not output the revision number or file type"
__fish_p4_register_command_option opened -s u -x -a '(__fish_print_p4_users)' -d "List only those files that were opened by user"
__fish_p4_register_command_option opened -s x -d "List all files that have the +l filetype over all servers"

# reopen
__fish_p4_register_command_option reopen -x -a '(__fish_print_p4_opened_files)' -d "Select opened files"
__fish_p4_register_command_option reopen -s c -x -a '(__fish_print_p4_pending_changelists default)' -d "Move files to changelist"
__fish_p4_register_command_option reopen -s t -x -a '(__fish_print_p4_file_types)' -d "Change file type"

# review @TODO

# shelve
__fish_p4_register_command_option shelve -a '(__fish_print_p4_opened_files)'
__fish_p4_register_command_option shelve -s a -x -a 'submitunchanged leaveunchanged' -d "Options: 'submitunchanged' shelves all files, 'leaveunchanged' shelves only changed files"
__fish_p4_register_command_option shelve -s c -x -a '(__fish_print_p4_pending_changelists)' -d "Changelist number"
__fish_p4_register_command_option shelve -s d -d 'Discard the shelved files'
__fish_p4_register_command_option shelve -s f -d 'Force overwrite of shelved files'
__fish_p4_register_command_option shelve -s i -d 'Read a changelist description from standard input'
__fish_p4_register_command_option shelve -s p -d "Promote a shelved change from an Edge server to a Commit server"
__fish_p4_register_command_option shelve -s r -d 'Replace all shelved files in the changelist with the files that are opened in your workspace'

# submit @TODO: parallel syntax
__fish_p4_register_command_option submit -a '(__fish_print_p4_opened_files)'
__fish_p4_register_command_option submit -s c -x -a '(__fish_print_p4_pending_changelists)' -d "Submit specific changelist"
__fish_p4_register_command_option submit -s d -x -d "Immediately submit the changelist with supplied description and bypass the interactive form"
__fish_p4_register_command_option submit -s e -x -a '(__fish_print_p4_shelved_changelists)' -d "Submit specific shelved changelist"
__fish_p4_register_command_option submit -s f -x -a '(__fish_print_p4_submit_options)' -d "Override the SubmitOptions setting in the p4 client form"
__fish_p4_register_command_option submit -s i -d "Read a changelist specification from standard input"
__fish_p4_register_command_option submit -l noretransfer -x -a '(__fish_print_p4_noretransfer_options)'
__fish_p4_register_command_option submit -l parallel -x -a '(__fish_print_p4_parallel_options "submit")' -d "Specify options for parallel file transfer"
__fish_p4_register_command_option submit -s r -d "Reopen files for edit in the default changelist after submission"
__fish_p4_register_command_option submit -s s -d "Allows jobs to be assigned arbitrary status values on submission of the changelist"

# unshelve @TODO: Streams
__fish_p4_register_command_option unshelve -s b -x -a '(__fish_print_p4_branches)' -d "Specifies a branch spec for unshelving from"
__fish_p4_register_command_option unshelve -s c -x -a '(__fish_print_p4_pending_changelists)' -d "Changelist number into which to unshelve"
__fish_p4_register_command_option unshelve -s f -d 'Force the overwriting of writable (but unopened) files'
__fish_p4_register_command_option unshelve -s n -d "Preview result of unshelve operation without restoring files to workspace"
__fish_p4_register_command_option unshelve -s P -x -a '(__fish_print_p4_streams)' -d "Unshelve to the specified parent stream"
__fish_p4_register_command_option unshelve -s s -x -a '(__fish_print_p4_shelved_changelists)' -d "Specify the source shelved pending changelist number"
__fish_p4_register_command_option unshelve -s S -x -a '(__fish_print_p4_streams)' -d "Specifies a stream spec for unshelving from"

#-----------------------------------------------------
#--> Jobs
#-----------------------------------------------------

# fix @TODO
# fixes @TODO
# job @TODO
# jobs @TODO
# jobspec @TODO

#-----------------------------------------------------
#--> Branching and Merging
#-----------------------------------------------------

# branch @TODO
# branches @TODO
# copy @TODO
# cstat @TODO

# integ, integrate @TODO -s fromFile is based on -b branchname, try resolving
for a in 'integ' 'integrate'
    __fish_p4_register_command_option $a -s b -x -a '(__fish_print_p4_branches)' -d "Integrate the files using the sourceFile/targetFile mappings included in the branch view of branchname. If the toFiles argument is included, include only those target files in the branch view that match the pattern specified by toFiles"
    __fish_p4_register_command_option $a -s n -d "Display the integrations this command would perform without actually performing them"
    __fish_p4_register_command_option $a -s v -d "Open files for branching without copying toFiles into the client workspace"
    __fish_p4_register_command_option $a -s c -x -a '(__fish_print_p4_pending_changelists)' -d "Open the toFiles for branch, integrate, or delete in the specified pending changelist"
    __fish_p4_register_command_option $a -s q -d "Quiet mode"
    __fish_p4_register_command_option $a -a '-Di' -d "f the source file has been deleted and re-added, revisions that precede the deletion will be considered to be part of the same source file"
    __fish_p4_register_command_option $a -s f -d "Force the integration on all revisions of fromFile and toFile, even if some revisions have been integrated in the past"
    __fish_p4_register_command_option $a -s h -d "Use the have revision"
    __fish_p4_register_command_option $a -s O -x -a '(__fish_print_p4_integrate_output_options)' -d "Specify output options"
    __fish_p4_register_command_option $a -s m -x -d "Limit the command to integrating only the first N files"
    __fish_p4_register_command_option $a -s R -x -a '(__fish_print_p4_integrate_resolve_options)' -d "Specify resolve options"
    __fish_p4_register_command_option $a -s s -r -d "Source file and revision"
    __fish_p4_register_command_option $a -s r -r -d "Reverse the mappings in the branch view, integrating from the target files to the source files"
    __fish_p4_register_command_option $a -s S -x -a '(__fish_print_p4_streams)' -d "Source stream"
    __fish_p4_register_command_option $a -s P -x -a '(__fish_print_p4_streams)' -d "Custom parent stream"
end

# integrated @TODO
# interchanges @TODO
# istat @TODO
# label @TODO
# labels @TODO
# labelsync @TODO
# list @TODO
# merge @TODO
# populate @TODO
# tag @TODO

# resolve
__fish_p4_register_command_option resolve -s a -x -a '(__fish_print_p4_resolve_options a)' -d "Resolve automatically"
__fish_p4_register_command_option resolve -s A -x -a '(__fish_print_p4_resolve_options A)' -d "Constrain the type of resolve to branching, deletion, file type change, or move/rename"
__fish_p4_register_command_option resolve -s d -x -a '(__fish_print_p4_resolve_options d)' -d "Ignore specified differences in whitespace or line-ending convention"
__fish_p4_register_command_option resolve -s f -d "Allow already resolved, but not yet submitted, files to be resolved again"
__fish_p4_register_command_option resolve -s n -d "List the files that need resolving without actually performing the resolve"
__fish_p4_register_command_option resolve -s N -d "Preview the operation with additional information about any non-content resolve actions that are scheduled"
__fish_p4_register_command_option resolve -s o -d "Output the base file name and revision to be used during the resolve"
__fish_p4_register_command_option resolve -s t -d "Force a three-way merge, even on binary (non-text) files"
__fish_p4_register_command_option resolve -s v -d "Include conflict markers in the file for all changes between yours and base, and between theirs and base"
__fish_p4_register_command_option resolve -s c -x -a '(__fish_print_p4_workspace_changelists)' -d "Limit the scope of the resolve operation to the files opened in the specified changelist number"

# resolved @TODO
# stream @TODO
# streams @TODO

#-----------------------------------------------------
#--> Administration
#-----------------------------------------------------

# admin @TODO
# archive @TODO
# cachepurge @TODO
# configure @TODO
# counter @TODO
# counters @TODO
# dbschema @TODO
# dbstat @TODO
# depot @TODO
# depots @TODO
# diskspace @TODO
# journals @TODO
# key @TODO
# keys @TODO
# license @TODO
# lockstat @TODO
# logappend @TODO
# logger @TODO
# logparse @TODO
# logrotate @TODO
# logschema @TODO
# logstat @TODO
# logtail @TODO
# monitor @TODO
# obliterate @TODO
# ping @TODO
# property @TODO
# proxy @TODO
# pull @TODO
# reload @TODO
# renameuser @TODO
# replicate @TODO
# restore @TODO
# reviews @TODO
# server @TODO
# serverid @TODO
# servers @TODO
# triggers @TODO
# typemap @TODO
# unload @TODO
# verify @TODO

#-----------------------------------------------------
#--> Security
#-----------------------------------------------------

# group @TODO
# groups @TODO
# login @TODO
# logout @TODO
# passwd @TODO
# protect @TODO
# protects @TODO
# tickets @TODO
# trust @TODO
# user @TODO
# users @TODO

#-----------------------------------------------------
#--> Environment
#-----------------------------------------------------

# set
__fish_p4_register_command_option set -x -a '(__fish_print_p4_env_vars)'
__fish_p4_register_command_option set -s q -d "Reduce the output"
__fish_p4_register_command_option set -s s -d "Set the value of the registry variable for the local machine"
__fish_p4_register_command_option set -s S -x -d "Set the value of the registry variables as used by the service"
