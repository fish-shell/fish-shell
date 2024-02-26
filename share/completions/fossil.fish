# fish completion for fossil
# http://www.fossil-scm.org/

function __fish_fossil
    command fossil $argv 2>/dev/null
end

function __fish_fossil_needs_command
    test (count (commandline -pxc)) -eq 1
end

function __fish_fossil_command
    set -l cmd (commandline -pxc)
    test (count $cmd) -gt 1
    and contains -- $cmd[2] $argv
end

function __fish_fossil_subcommand
    set -l cmd (commandline -pxc)
    test (count $cmd) -eq 2
    and test $argv[1] = $cmd[2]
end

function __fish_fossil_subsubcommand
    set -l cmd (commandline -pxc)
    test (count $cmd) -ge 3
    and test $argv[1] = $cmd[2]
    and test $argv[2] = $cmd[3]
end

function __fish_fossil_subsubcommand_only
    set -l cmd (commandline -pxc)
    test (count $cmd) -eq 3
    and test $argv[1] = $cmd[2]
    and test $argv[2] = $cmd[3]
end

function __fish_fossil_subsubsubcommand_only
    set -l cmd (commandline -pxc)
    test (count $cmd) -eq 4
    and test $argv[1] = $cmd[2]
    and test $argv[2] = $cmd[3]
    and test $argv[3] = $cmd[4]
end

# add
complete -c fossil -n __fish_fossil_needs_command -a add -x -d 'Add files to checkout'
complete -c fossil -n '__fish_fossil_command add' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command add' -l dotfiles -d 'Include dotfiles'
complete -c fossil -n '__fish_fossil_command add' -l ignore -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command add' -l clean -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command add' -a '(__fish_fossil extra --rel-paths)' -x -d File

# addremove
complete -c fossil -n __fish_fossil_needs_command -a addremove -f -d 'Remove and add files to checkout'
complete -c fossil -n '__fish_fossil_command addremove' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command addremove' -l dotfiles -d 'Include dotfiles'
complete -c fossil -n '__fish_fossil_command addremove' -l ignore -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command addremove' -l clean -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command addremove' -s n -l dry-run -d 'Display actions without running'

# all
complete -c fossil -n __fish_fossil_needs_command -a all -f -d 'Check all repositories'
complete -c fossil -n '__fish_fossil_subcommand all' -a changes -f -d 'List changes'
complete -c fossil -n '__fish_fossil_subcommand all' -a ignore -d 'Ignore repository'
complete -c fossil -n '__fish_fossil_subsubcommand all ignore' -x -a '(__fish_fossil all ls)' -d 'Ignore repository'
complete -c fossil -n '__fish_fossil_subcommand all' -a 'list ls' -d 'Display locations'
complete -c fossil -n '__fish_fossil_subcommand all' -a pull -d 'Pull repositories'
complete -c fossil -n '__fish_fossil_subcommand all' -a push -d 'Push repositories'
complete -c fossil -n '__fish_fossil_subcommand all' -a rebuild -d 'Rebuild repositories'
complete -c fossil -n '__fish_fossil_subcommand all' -a sync -d 'Sync repositories'

# annotate
complete -c fossil -n __fish_fossil_needs_command -a annotate -d 'Shows file modifications'
complete -c fossil -n '__fish_fossil_command annotate' -l filevers -d 'Show file versions'
complete -c fossil -n '__fish_fossil_command annotate' -s l -l log -d 'List all analyzed versions'
complete -c fossil -n '__fish_fossil_command annotate' -s n -l limit -x -d 'Limit analyzed versions'

# bisect
complete -c fossil -n __fish_fossil_needs_command -a bisect -f -d 'Find regressions'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a bad -x -d 'Identify version as not working'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a good -x -d 'Identify version as working'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a log -d 'Show log of bisects in test order'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a chart -d 'Show log of bisects in check-in order'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a next -d 'Skip version'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a options -d 'Show bisect options'
complete -c fossil -n '__fish_fossil_subsubcommand_only bisect options' -x -a auto-next -d 'Automatically run bisect next'
complete -c fossil -n '__fish_fossil_subsubcommand_only bisect options' -x -a direct-only -d 'Follow only primary parent-child links'
complete -c fossil -n '__fish_fossil_subsubcommand_only bisect options' -x -a display -d 'Command to show after bisect next'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options auto-next' -x -a 'on off' -d 'Automatically run bisect next'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options direct-only' -x -a 'on off' -d 'Follow only primary parent-child links'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options display' -x -a chart -d 'Show log of bisects in check-in order'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options display' -x -a log -d 'Show log of bisects in test order'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options display' -x -a status -d 'List versions between bad and good'
complete -c fossil -n '__fish_fossil_subsubsubcommand_only bisect options display' -x -a none -d 'Don\'t show anything'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a reset -d 'Reinitialize bisect'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a 'vlist ls status' -d 'List versions between bad and good'
complete -c fossil -n '__fish_fossil_subsubcommand bisect vlist' -s a -l all -d 'Show all versions'
complete -c fossil -n '__fish_fossil_subsubcommand bisect ls' -s a -l all -d 'Show all versions'
complete -c fossil -n '__fish_fossil_subsubcommand bisect status' -s a -l all -d 'Show all versions'
complete -c fossil -n '__fish_fossil_subcommand bisect' -a undo -d 'Undo latest bad/good command.'

# branch
complete -c fossil -n __fish_fossil_needs_command -a branch -f -d 'Create a new branch'
complete -c fossil -n '__fish_fossil_subcommand branch' -a new -x -d 'Create new branch'
complete -c fossil -n '__fish_fossil_subsubcommand branch new' -l private -d 'Make branch local'
complete -c fossil -n '__fish_fossil_subsubcommand branch new' -l bgcolor -x -d 'Set background color'
complete -c fossil -n '__fish_fossil_subsubcommand branch new' -l nosign -d 'Don\'t sign the branch with GPG'
complete -c fossil -n '__fish_fossil_subsubcommand branch new' -l date-override -x -d 'Override date'
complete -c fossil -n '__fish_fossil_subsubcommand branch new' -l user-override -x -d 'Override user'
complete -c fossil -n '__fish_fossil_subcommand branch' -a 'list ls' -x -d 'List branches'
complete -c fossil -n '__fish_fossil_subsubcommand branch list' -s a -l all -d 'Show all branches'
complete -c fossil -n '__fish_fossil_subsubcommand branch ls' -s a -l all -d 'Show all branches'
complete -c fossil -n '__fish_fossil_subsubcommand branch list' -s c -l closed -d 'Show closed branches'
complete -c fossil -n '__fish_fossil_subsubcommand branch ls' -s c -l closed -d 'Show closed branches'
complete -c fossil -n '__fish_fossil_command branch' -s R -l repository -r -d 'Run command on repository'

# cat
complete -c fossil -n __fish_fossil_needs_command -a cat -d 'Print a file'
complete -c fossil -n '__fish_fossil_command cat' -s R -l repository -r -d 'Run command on repository'
complete -c fossil -n '__fish_fossil_command cat' -s r -x -a '(__fish_fossil tag list)' -d 'Print specific revision'

# changes
complete -c fossil -n __fish_fossil_needs_command -f -a changes -d 'List local changes'
complete -c fossil -n '__fish_fossil_command changes' -l abs-paths -d 'Display absolute paths'
complete -c fossil -n '__fish_fossil_command changes' -l rel-paths -d 'Display relative paths'
complete -c fossil -n '__fish_fossil_command changes' -l sha1sum -d 'Verify file status using SHA1'
complete -c fossil -n '__fish_fossil_command changes' -l header -d 'Identify the repository if there are changes'
complete -c fossil -n '__fish_fossil_command changes' -s v -l verbose -d 'Say (none) if there are no changes'

# checkout
complete -c fossil -n __fish_fossil_needs_command -f -a 'co checkout' -d 'Checkout version'
complete -c fossil -n '__fish_fossil_command co checkout' -x -a '(__fish_fossil tag list)' -d 'Version to check out'
complete -c fossil -n '__fish_fossil_command co checkout' -l force -d 'Ignore edited files'
complete -c fossil -n '__fish_fossil_command co checkout' -l force-missing -d 'Ignore missing content'
complete -c fossil -n '__fish_fossil_command co checkout' -l keep -d 'Only update the manifest'
complete -c fossil -n '__fish_fossil_command co checkout' -l latest -d 'Update to latest version'

# clean
complete -c fossil -n __fish_fossil_needs_command -a clean -d 'Delete all extra files'
complete -c fossil -n '__fish_fossil_command clean' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command clean' -l dotfiles -d 'Include dotfiles'
complete -c fossil -n '__fish_fossil_command clean' -l ignore -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command clean' -l clean -r -d 'Files to clean without prompting'
complete -c fossil -n '__fish_fossil_command clean' -s f -l force -d 'Remove without prompting'
complete -c fossil -n '__fish_fossil_command clean' -s n -l dry-run -d 'Display actions without running'
complete -c fossil -n '__fish_fossil_command clean' -l temp -d 'Only remove Fossil temporary files'
complete -c fossil -n '__fish_fossil_command clean' -s v -l verbose -d 'Show removed files'

# clone
complete -c fossil -n __fish_fossil_needs_command -a clone -d 'Clone repository'
complete -c fossil -n '__fish_fossil_command clone' -s A -l admin-user -x -d 'Make username an administrator'
complete -c fossil -n '__fish_fossil_command clone' -l private -d 'Clone private branches'
complete -c fossil -n '__fish_fossil_command clone' -l ssl-identity -r -d 'Use SSL identity'

# commit
complete -c fossil -n __fish_fossil_needs_command -a 'ci commit' -d 'Create new revision'
complete -c fossil -n '__fish_fossil_command ci commit' -a '(__fish_fossil changes --rel-paths | cut -c12-)' -x -d File
complete -c fossil -n '__fish_fossil_command ci commit' -l allow-conflict -d 'Allow unresolved merge conflicts'
complete -c fossil -n '__fish_fossil_command ci commit' -l allow-empty -d 'Allow empty check-ins'
complete -c fossil -n '__fish_fossil_command ci commit' -l allow-fork -d 'Allow forking'
complete -c fossil -n '__fish_fossil_command ci commit' -l allow-older -d 'Allow commit older than ancestor'
complete -c fossil -n '__fish_fossil_command ci commit' -l baseline -d 'Use a baseline manifest'
complete -c fossil -n '__fish_fossil_command ci commit' -l bgcolor -x -d 'Apply color to check-in'
complete -c fossil -n '__fish_fossil_command ci commit' -l branch -x -d 'Check-in to new branch'
complete -c fossil -n '__fish_fossil_command ci commit' -l branchcolor -x -d 'Apply color to branch'
complete -c fossil -n '__fish_fossil_command ci commit' -l close -d 'Close the branch'
complete -c fossil -n '__fish_fossil_command ci commit' -l delta -d 'Use a delta manifest'
complete -c fossil -n '__fish_fossil_command ci commit' -s m -l comment -x -d 'Commit comment'
complete -c fossil -n '__fish_fossil_command ci commit' -s M -l message-file -r -d 'Read commit comment from a file'
complete -c fossil -n '__fish_fossil_command ci commit' -l mimetype -x -d 'Mimetype of commit comment'
complete -c fossil -n '__fish_fossil_command ci commit' -s n -l dry-run -d 'Display actions without running'
complete -c fossil -n '__fish_fossil_command ci commit' -l no-warnings -d 'Omit all warnings'
complete -c fossil -n '__fish_fossil_command ci commit' -l nosign -d 'Don\'t sign the branch with GPG'
complete -c fossil -n '__fish_fossil_command ci commit' -l private -d 'Don\'t sync the changes'
complete -c fossil -n '__fish_fossil_command ci commit' -l tag -x -d 'Assign a tag to the checkin'

# diff
complete -c fossil -n __fish_fossil_needs_command -a 'diff gdiff' -d 'Show differences'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l binary -r -d 'Binary files glob pattern'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l branch -x -a '(__fish_fossil tag list)' -d 'Show diff of branch'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l brief -d 'Show file names only'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s c -l context -x -d 'Context lines'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l diff-binary -a 'yes no' -x -d 'Include binary files'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s r -l from -x -a '(__fish_fossil tag list)' -d 'Select revision to compare with'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s i -l internal -d 'Use internal diff logic'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s y -l side-by-side -d 'Side-by-side diff'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l tk -d 'Launch GUI'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l to -x -a '(__fish_fossil tag list)' -d 'Select revision to compare to'
complete -c fossil -n '__fish_fossil_command diff gdiff' -l unified -d 'Unified diff'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s v -l verbose -d 'Output complete text'
complete -c fossil -n '__fish_fossil_command diff gdiff' -s W -l width -x -d 'Line width in side-by-side diff'

# export
complete -c fossil -n __fish_fossil_needs_command -a export -d 'Export repository to git'
complete -c fossil -n '__fish_fossil_command export' -l export-marks -r -d 'Export rids of exported data to file'
complete -c fossil -n '__fish_fossil_command export' -l import-marks -r -d 'Read rids of data to ignore from file'
complete -c fossil -n '__fish_fossil_command export' -s R -l repository -r -d 'Run command on repository'

# extras
complete -c fossil -n __fish_fossil_needs_command -a extras -d 'Show files that aren\'t part of checkout'
complete -c fossil -n '__fish_fossil_command extras' -l abs-paths -d 'Display absolute paths'
complete -c fossil -n '__fish_fossil_command extras' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command extras' -l dotfiles -d 'Include dotfiles'
complete -c fossil -n '__fish_fossil_command extras' -l ignore -r -d 'Files to ignore'
complete -c fossil -n '__fish_fossil_command extras' -l rel-paths -d 'Display relative paths'

# finfo
complete -c fossil -n __fish_fossil_needs_command -a finfo -d 'Print complete file history'
complete -c fossil -n '__fish_fossil_command finfo' -s b -l brief -d 'Display one-line summary'
complete -c fossil -n '__fish_fossil_command finfo' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command finfo' -s l -l log -d 'Select log mode (default)'
complete -c fossil -n '__fish_fossil_command finfo' -s n -l limit -x -d 'Limit analyzed versions'
complete -c fossil -n '__fish_fossil_command finfo' -l offset -x -d 'Skip changes'
complete -c fossil -n '__fish_fossil_command finfo' -s p -l print -d 'Select print mode'
complete -c fossil -n '__fish_fossil_command finfo' -s r -l revision -x -a '(__fish_fossil tag list)' -d 'Print specific revision'
complete -c fossil -n '__fish_fossil_command finfo' -s s -l status -d 'Select status mode'

# help
complete -c fossil -n __fish_fossil_needs_command -f -a help -d 'Display help'
complete -c fossil -n '__fish_fossil_command help' -s a -l all -d 'Show main and auxiliary commands'
complete -c fossil -n '__fish_fossil_command help' -s t -l test -d 'Show test commands only'
complete -c fossil -n '__fish_fossil_command help' -s x -l aux -d 'Show auxiliary commands only'
complete -c fossil -n '__fish_fossil_command help' -s w -l www -d 'Show list of web UI pages'

# import
complete -c fossil -n __fish_fossil_needs_command -a import -d 'Import repository from git'
complete -c fossil -n '__fish_fossil_command import' -l incremental -d 'Allow importing into existing repository'

# info
complete -c fossil -n __fish_fossil_needs_command -a import -d 'Provide information about object'
complete -c fossil -n '__fish_fossil_command import' -s R -l repository -r -d 'Run command on repository'
complete -c fossil -n '__fish_fossil_command import' -s v -l verbose -d 'Show extra information'

# init
complete -c fossil -n __fish_fossil_needs_command -a 'init new' -d 'Create a repository'
complete -c fossil -n '__fish_fossil_command init new' -l template -r -d 'Copy settings from repository'
complete -c fossil -n '__fish_fossil_command init new' -s A -l admin-user -r -d 'Make username an administrator'
complete -c fossil -n '__fish_fossil_command init new' -l date-override -x -d 'Override date'

# json
complete -c fossil -n __fish_fossil_needs_command -a json -d 'Make JSON request'
complete -c fossil -n '__fish_fossil_command json' -s R -l repository -r -d 'Run command on repository'
# TODO more

# ls
complete -c fossil -n __fish_fossil_needs_command -a ls -d 'List files'
complete -c fossil -n '__fish_fossil_command ls' -s r -x -a '(__fish_fossil tag list)' -d Tag
complete -c fossil -n '__fish_fossil_command ls' -l age -d 'Show commit time'
complete -c fossil -n '__fish_fossil_command ls' -s v -l verbose -d 'Provide extra information'

# merge
complete -c fossil -n __fish_fossil_needs_command -a merge -d 'Merge commits'
complete -c fossil -n '__fish_fossil_command merge' -a '(__fish_fossil tag list)' -d Tag
complete -c fossil -n '__fish_fossil_command merge' -l baseline -a '(__fish_fossil tag list)' -x -d 'Use baseline'
complete -c fossil -n '__fish_fossil_command merge' -l binary -r -d 'Binary files glob pattern'
complete -c fossil -n '__fish_fossil_command merge' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command merge' -s f -l force -d 'Allow empty merge'
complete -c fossil -n '__fish_fossil_command merge' -l integrate -d 'Close merged branch'
complete -c fossil -n '__fish_fossil_command merge' -s n -l dry-run -d 'Display actions without running'
complete -c fossil -n '__fish_fossil_command merge' -s v -l verbose -d 'Show extra information'

# mv
complete -c fossil -n __fish_fossil_needs_command -a mv -d 'Move file'
complete -c fossil -n '__fish_fossil_command mv' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'

# open
complete -c fossil -n __fish_fossil_needs_command -a open -d 'Open repository'
complete -c fossil -n '__fish_fossil_command open' -l keep -d 'Only modify manifest'
complete -c fossil -n '__fish_fossil_command open' -l nested -d 'Allow opening inside an opened repository'

# pull
complete -c fossil -n __fish_fossil_needs_command -a pull -d 'Pull from a repository'
complete -c fossil -n '__fish_fossil_command pull' -s R -l repository -r -d 'Run command on repository'
complete -c fossil -n '__fish_fossil_command pull' -l private -r -d 'Pull private branches'

# push
complete -c fossil -n __fish_fossil_needs_command -a push -d 'Push into a repository'
complete -c fossil -n '__fish_fossil_command push' -s R -l repository -r -d 'Run command on repository'
complete -c fossil -n '__fish_fossil_command push' -l private -d 'Pull private branches'

# rebuild
complete -c fossil -n __fish_fossil_needs_command -a rebuild -d 'Rebuild a repository'
complete -c fossil -n '__fish_fossil_command rebuild' -l cluster -d 'Compute clusters'
complete -c fossil -n '__fish_fossil_command rebuild' -l compress -d 'Compress database'
complete -c fossil -n '__fish_fossil_command rebuild' -l force -d 'Force rebuild even with errors'
complete -c fossil -n '__fish_fossil_command rebuild' -l noverify -d 'Skip BLOB table verification'
complete -c fossil -n '__fish_fossil_command rebuild' -l pagesize -x -a '512 1024 2048 4096 8192 16384 32768 65536' -d 'Set the database pagesize'
complete -c fossil -n '__fish_fossil_command rebuild' -l randomize -d 'Scan in random order'
complete -c fossil -n '__fish_fossil_command rebuild' -l vacuum -d 'Run VACUUM'
complete -c fossil -n '__fish_fossil_command rebuild' -l deanalyze -d 'Remove ANALYZE tables'
complete -c fossil -n '__fish_fossil_command rebuild' -l analyze -d 'Run ANALYZE'
complete -c fossil -n '__fish_fossil_command rebuild' -l wal -d 'Set Write-Ahead-Log journalling'
complete -c fossil -n '__fish_fossil_command rebuild' -l stats -d 'Show statistics'

# remote-url
complete -c fossil -n __fish_fossil_needs_command -a remote-url -d 'Default server URL'

# revert
complete -c fossil -n __fish_fossil_needs_command -f -a revert -d 'Revert a commit'
complete -c fossil -n '__fish_fossil_command revert' -a '(__fish_fossil tag list)' -d Tag
complete -c fossil -n '__fish_fossil_command revert' -s r -x -a '(__fish_fossil tag list)' -d 'Revert back to given revision'

# rm
complete -c fossil -n __fish_fossil_needs_command -a 'rm delete' -d 'Remove a file from repository'
complete -c fossil -n '__fish_fossil_command rm delete' -l hard -d 'Remove files from this checkout'
complete -c fossil -n '__fish_fossil_command rm delete' -l soft -d 'Skip removing files from this checkout'
complete -c fossil -n '__fish_fossil_command rm delete' -s n -l dry-run -d 'Display actions without runing'
complete -c fossil -n '__fish_fossil_command rm delete' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'

# settings
complete -c fossil -n __fish_fossil_needs_command -a settings -f -d 'Manage settings'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a access-log -d 'Log accesses'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a allow-symlinks -d 'Allow symbolic links'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a auto-captcha -d 'Allow automatically filling CAPTCHA'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a auto-hyperlink -d 'Use JavaScript to enable hyperlinks'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a auto-shun -d 'Pull list of shunned references from server'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a autosync -d 'Automatically sync the repository'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a binary-glob -d 'Binary files glob pattern'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a case-sensitive -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a clean-glob -d 'Files to clean without prompting'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a clearsign -d 'Sign commits using GPG'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a crnl-glob -d 'Non-standard line endings allowed glob pattern'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a default-perms -d 'Permissions given to new users'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a diff-binary -d 'Allow binary files to be diffed'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a diff-command -d 'External diff command'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a dont-push -d 'Disallow pushing to the repository'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a editor -d 'Text editor for writing check-in comments'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a empty-dirs -d 'List of empty directories'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a encoding-glob -d 'Non-UTF-8 files glob pattern'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a gdiff-command -d 'Command to use for graphical diff'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a gmerge-command -d 'Command to use for graphical merge'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a http-port -d 'HTTP port for fossil ui'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a https-login -d 'Send login credentials using HTTPS'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a ignore-glob -d 'Files to ignore glob pattern'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a keep-glob -d 'Files to keep when cleaning'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a localauth -d 'Require authentication for localhost logins'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a main-branch -d 'Primary branch'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a manifest -d 'Automatically create manifest files'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a max-upload -d 'HTTP request size limit'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a mtime-changes -d 'Use modification times'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a pgp-command -d 'PGP command'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a proxy -d 'HTTP proxy URL'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a relative-paths -d 'Report relative paths'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a repo-cksum -d 'Compute checksums over all files'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a self-register -d 'Allow users to register themselves'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a ssh-command -d 'Command to use for SSH protocol'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a ssl-ca-location -d 'Location of SSL CA root certificates'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a ssl-identity -d 'SSL private certificate path'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a tcl -d 'Allow Tcl scripting'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a tcl-setup -d 'Tcl initialization script'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a th1-setup -d 'TH1 initialization script'
complete -c fossil -n '__fish_fossil_subcommand settings' -x -a web-browser -d 'Web browser name'
complete -c fossil -n '__fish_fossil_command settings' -l global -d 'Set globally'

# sqlite3
complete -c fossil -n __fish_fossil_needs_command -a sqlite3 -f -d 'Run SQL commands'

# stash
complete -c fossil -n __fish_fossil_needs_command -a stash -f -d 'Manage stashes'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a 'save snapshot' -d 'Save current changes'
complete -c fossil -n '__fish_fossil_subsubcommand stash save' -s m -l comment -x -d 'Stash comment'
complete -c fossil -n '__fish_fossil_subsubcommand stash snapshot' -s m -l comment -x -d 'Stash comment'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a 'list ls' -d 'List all stashes'
complete -c fossil -n '__fish_fossil_subsubcommand stash list' -s v -l verbose -d 'Show information about files'
complete -c fossil -n '__fish_fossil_subsubcommand stash ls' -s v -l verbose -d 'Show information about files'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a show -d 'Show stash contents'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a pop -d 'Pop last stash'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a apply -d 'Apply stash'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a goto -d 'Updates to stash state'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a 'drop rm' -d 'Forget about stash'
complete -c fossil -n '__fish_fossil_subsubcommand stash drop' -s a -l all -d 'Forget ALL stashes'
complete -c fossil -n '__fish_fossil_subsubcommand stash rm' -s a -l all -d 'Forget ALL stashes'
complete -c fossil -n '__fish_fossil_subcommand stash' -x -a goto -d 'Compare stash'

# status
complete -c fossil -n __fish_fossil_needs_command -a status -f -d 'Show status'
complete -c fossil -n '__fish_fossil_command status' -l abs-paths -d 'Display absolute paths'
complete -c fossil -n '__fish_fossil_command status' -l rel-paths -d 'Display relative paths'
complete -c fossil -n '__fish_fossil_command status' -l sha1sum -d 'Verify file status using SHA1'

# sync
complete -c fossil -n __fish_fossil_needs_command -a sync -d 'Sync with a repository'
complete -c fossil -n '__fish_fossil_command sync' -s R -l repository -r -d 'Run command on repository'
complete -c fossil -n '__fish_fossil_command sync' -l private -r -d 'Sync private branches'

# tag
complete -c fossil -n __fish_fossil_needs_command -a tag -d 'Manage tags'
complete -c fossil -n '__fish_fossil_subcommand tag' -x -a add -d 'Add tag to check-in'
complete -c fossil -n '__fish_fossil_subsubcommand tag add' -l raw -d 'Add raw tag'
complete -c fossil -n '__fish_fossil_subsubcommand tag add' -l propagate -d 'Propagate tag'
complete -c fossil -n '__fish_fossil_subcommand tag' -x -a remove -d 'Remove tag from check-in'
complete -c fossil -n '__fish_fossil_subsubcommand tag remove' -a '(__fish_fossil tag list)' -d Tag
complete -c fossil -n '__fish_fossil_subsubcommand tag remove' -l raw -d 'Remove raw tag'
complete -c fossil -n '__fish_fossil_subcommand tag' -x -a find -d 'Find tag'
complete -c fossil -n '__fish_fossil_subsubcommand tag find' -l raw -d 'Find raw tag'
complete -c fossil -n '__fish_fossil_subsubcommand tag find' -s t -l type -x -a 'ci e' -d 'Find tag type'
complete -c fossil -n '__fish_fossil_subsubcommand tag find' -s n -l limit -x -d 'Limit number of tags'
complete -c fossil -n '__fish_fossil_subsubcommand tag find' -a '(__fish_fossil tag list)' -d Tag
complete -c fossil -n '__fish_fossil_subcommand tag' -x -a list -d 'List tags'
complete -c fossil -n '__fish_fossil_subsubcommand tag list' -l raw -d 'List raw tags'

# timeline
complete -c fossil -n __fish_fossil_needs_command -a timeline -d 'Show timeline'
complete -c fossil -n '__fish_fossil_command timeline' -s n -l limit -x -d 'Limit timeline entries'
complete -c fossil -n '__fish_fossil_command timeline' -s t -l type -x -a 'ci e t w' -d 'Output only event type'
complete -c fossil -n '__fish_fossil_command timeline' -s v -l verbose -d 'Output list of files changed'

# ui
complete -c fossil -n __fish_fossil_needs_command -a ui -d 'Open web UI'
complete -c fossil -n __fish_fossil_needs_command -a server -d 'Start web server'
complete -c fossil -n '__fish_fossil_command ui server' -l localauth -d 'Enable automatic login for localhost'
complete -c fossil -n '__fish_fossil_command server' -l localhost -d 'Only listen on localhost'
complete -c fossil -n '__fish_fossil_command ui server' -s P -l port -d 'Port to listen on'
complete -c fossil -n '__fish_fossil_command ui server' -l th-trace -d 'Trace TH1 execution'
complete -c fossil -n '__fish_fossil_command ui server' -l baseurl -d 'Use base URL'
complete -c fossil -n '__fish_fossil_command ui server' -l notfound -d Redirect
complete -c fossil -n '__fish_fossil_command ui server' -l files -d 'Static files glob'
complete -c fossil -n '__fish_fossil_command ui server' -l scgi -d 'Use SCGI rather than HTTP'

# undo
complete -c fossil -n __fish_fossil_needs_command -a undo -d 'Undo the changes'
complete -c fossil -n '__fish_fossil_command undo' -s n -l dry-run -d 'Display actions without running'

# update
complete -c fossil -n __fish_fossil_needs_command -a update -d 'Update version'
complete -c fossil -n '__fish_fossil_command update' -l case-sensitive -x -a 'yes no' -d 'Case insensitive file matching'
complete -c fossil -n '__fish_fossil_command update' -l debug -d 'Print debug information'
complete -c fossil -n '__fish_fossil_command update' -l latest -d 'Update to latest version'
complete -c fossil -n '__fish_fossil_command update' -s n -l dry-run -d 'Display actions without running'
complete -c fossil -n '__fish_fossil_command update' -s v -l verbose -d 'Print information about all files'

# version
complete -c fossil -n __fish_fossil_needs_command -a version -d 'Print fossil version'
complete -c fossil -n '__fish_fossil_command version' -s v -l verbose -d 'Print version of optional features'

# --help
complete -c fossil -l help -d 'Print help'
