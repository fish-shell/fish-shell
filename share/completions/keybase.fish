#Keybase 5.0.0

function __fish_keybase_commandline_ends_with
    set -l line (commandline -poc)
    for i in (seq -1 -1 -(count $argv))
        if test "$line[$i]" != "$argv[$i]"; return 1; end
    end
end

#variables
set -l ends __fish_keybase_commandline_ends_with
set -l seen __fish_seen_subcommand_from
#L0
set -l c_keybase account blocks bot chat ctl currency decrypt deprovision device encrypt follow fs git h help id list-followers list-following log login logout oneshot paperkey passphrase pgp ping prove rekey selfprovision service sign signup sigs status team track unfollow untrack update verify version wallet
#L1
set -l c_keybase_account delete email h help lockdown recover-username upload-avatar
set -l c_keybase_blocks h help list-users
set -l c_keybase_bot h help signup token
set -l c_keybase_chat add-to-channel api api-listen create-channel delete-channel delete-history download h help hide join-channel leave-channel list list-channels list-members list-unread ls lsur min-writer-role mute notification-settings read readd-member rename-channel report retention-policy search search-regexp send upload
set -l c_keybase_ctl app-exit autostart h help init log-rotate redirector reload restart start stop watchdog watchdog2
set -l c_keybase_currency add h help
set -l c_keybase_device add h help list remove
set -l c_keybase_fs clear-conflicts cp debug finish-resolving-conflicts get-status h help history kill ln ls mkdir mv ps quota read recover reset rm set-debug-level stat sync uploads write			
set -l c_keybase_git create delete gc h help list settings
set -l c_keybase_help advanced gpg keyring tor
set -l c_keybase_log h help send
set -l c_keybase_passphrase change check h help recover remember set
set -l c_keybase_pgp decrypt drop encrypt export gen h help import list pull pull-private purge push-private select sign update verify
set -l c_keybase_rekey h help paper status
set -l c_keybase_sigs h help list revoke
set -l c_keybase_team accept-invite add-member add-members-bulk api create delete edit-member h help ignore-request leave list-members list-memberships list-requests remove-member rename request-access settings show-tree
set -l c_keybase_update check check-in-use notify
set -l c_keybase_wallet accounts add-trustline api asset-search balances cancel cancel-request change-trustline-limit delete-trustline detail details export get-inflation get-started h help history import list lookup merge popular-assets reclaim rename request send send-path-payment set-currency set-inflation set-primary sign
#L2
set -l c_keybase_fs_sync disable enable h help show
#...

#global options
complete -c keybase -f -n "not $seen $c_keybase" -s h -l help -d "Show help"
#...

#commands
#L0
complete -c keybase -f -n "$ends keybase" -a "$c_keybase"
#L1
complete -c keybase -f -n "$ends keybase account" -a "$c_keybase_account"
complete -c keybase -f -n "$ends keybase blocks" -a "$c_keybase_blocks"
complete -c keybase -f -n "$ends keybase bot" -a "$c_keybase_bot"
complete -c keybase -f -n "$ends keybase chat" -a "$c_keybase_chat"
complete -c keybase -f -n "$ends keybase ctl" -a "$c_keybase_ctl"
complete -c keybase -f -n "$ends keybase currency" -a "$c_keybase_currency"
complete -c keybase -f -n "$ends keybase device" -a "$c_keybase_device"
complete -c keybase -f -n "$ends keybase fs" -a "$c_keybase_fs"
complete -c keybase -f -n "$ends keybase git" -a "$c_keybase_git"
complete -c keybase -f -n "$ends keybase h" -a "$c_keybase_help"
complete -c keybase -f -n "$ends keybase help" -a "$c_keybase_help"
complete -c keybase -f -n "$ends keybase log" -a "$c_keybase_log"
complete -c keybase -f -n "$ends keybase passphrase" -a "$c_keybase_passphrase"
complete -c keybase -f -n "$ends keybase pgp" -a "$c_keybase_pgp"
complete -c keybase -f -n "$ends keybase rekey" -a "$c_keybase_rekey"
complete -c keybase -f -n "$ends keybase sigs" -a "$c_keybase_sigs"
complete -c keybase -f -n "$ends keybase team" -a "$c_keybase_team"
complete -c keybase -f -n "$ends keybase update" -a "$c_keybase_update"
complete -c keybase -f -n "$ends keybase wallet" -a "$c_keybase_wallet"
#L2
complete -c keybase -f -n "$ends keybase fs sync" -a "$c_keybase_fs_sync"


#command options
#...
