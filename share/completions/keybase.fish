#Keybase 5.6.1

function __fish_keybase_line_ends_with
    set -l line (commandline -pxc | string match -v -r '^-')
    for i in (seq -1 -1 -(count $argv))
        if test "$line[$i]" != "$argv[$i]"
            return 1
        end
    end
end

#variables
set -l ends __fish_keybase_line_ends_with
set -l seen __fish_seen_subcommand_from
#L1
set -l keybase account blocks bot chat config ctl currency decrypt deprovision device encrypt follow fs git h help id kvstore list-followers list-following log login logout oneshot paperkey passphrase pgp ping prove rekey selfprovision service sign signup sigs status team track unfollow untrack update verify version wallet whoami
#L2
set -l keybase_account contact-settings delete email h help lockdown recover-username upload-avatar
set -l keybase_blocks h help list-users list-teams
set -l keybase_bot h help signup token
set -l keybase_chat add-bot-member add-to-channel api api-listen bot-member-settings clear-commands conv-info create-channel default-channels delete-channel delete-history download edit-bot-member featured-bots h help hide join-channel leave-channel list list-channels list-members list-unread ls lsur min-writer-role mute notification-settings read readd-member remove-bot-member rename-channel report retention-policy search search-bots search-regexp send upload
set -l keybase_config get h help info set
set -l keybase_ctl app-exit autostart h help init log-rotate redirector reload restart start stop wait watchdog watchdog2
set -l keybase_currency add h help
set -l keybase_device add h help list remove
set -l keybase_fs clear-conflicts cp debug finish-resolving-conflicts get-status h help history kill ln ls mkdir mv ps quota read recover reset rm set-debug-level stat sync uploads write
set -l keybase_git create delete gc h help lfs-config list settings
set -l keybase_help advanced gpg keyring tor
set -l keybase_kvstore api h help
set -l keybase_log h help send
set -l keybase_passphrase change check h help recover remember set
set -l keybase_pgp decrypt drop encrypt export gen h help import list pull pull-private purge push-private select sign update verify
set -l keybase_rekey h help paper status
set -l keybase_sigs h help list revoke
set -l keybase_team accept-invite add-member add-members-bulk api bot-settings create delete edit-member h help ignore-request leave list-members list-memberships list-requests remove-member rename request-access search settings show-tree
set -l keybase_update check check-in-use notify
set -l keybase_wallet accounts add-trustline api asset-search balances cancel cancel-request change-trustline-limit delete-trustline detail details export get-inflation get-started h help history import list lookup merge popular-assets reclaim rename request send send-path-payment set-currency set-inflation set-primary sign
#L3
set -l keybase_account_email add delete edit h help list send-verification-email set-primary set-visibility
set -l keybase_fs_debug deobfuscate dump h help obfuscate
set -l keybase_fs_sync disable enable h help show
#...

#global options
complete -c keybase -f -n "$ends keybase" -l api-dump-unsafe
complete -c keybase -f -n "$ends keybase" -l api-timeout
complete -c keybase -f -n "$ends keybase" -l api-uri-path-prefix
complete -c keybase -f -n "$ends keybase" -l app-start-mode
complete -c keybase -f -n "$ends keybase" -l auto-fork
complete -c keybase -f -n "$ends keybase" -l bg-identifier-disabled
complete -c keybase -f -n "$ends keybase" -l chat-db
complete -c keybase -f -n "$ends keybase" -l code-signing-kids
complete -c keybase -f -n "$ends keybase" -l config-file -s c
complete -c keybase -f -n "$ends keybase" -l db
complete -c keybase -f -n "$ends keybase" -l debug -s d
complete -c keybase -f -n "$ends keybase" -l debug-journeycard
complete -c keybase -f -n "$ends keybase" -l disable-bg-conv-loader
complete -c keybase -f -n "$ends keybase" -l disable-cert-pinning
complete -c keybase -f -n "$ends keybase" -l disable-merkle-auditor
complete -c keybase -f -n "$ends keybase" -l disable-search-indexer
complete -c keybase -f -n "$ends keybase" -l disable-team-auditor
complete -c keybase -f -n "$ends keybase" -l disable-team-box-auditor
complete -c keybase -f -n "$ends keybase" -l display-raw-untrusted-output
complete -c keybase -f -n "$ends keybase" -l ek-log-file
complete -c keybase -f -n "$ends keybase" -l enable-bot-lite-mode
complete -c keybase -f -n "$ends keybase" -l extra-net-logging
complete -c keybase -f -n "$ends keybase" -l features
complete -c keybase -f -n "$ends keybase" -l force-linux-keyring
complete -c keybase -f -n "$ends keybase" -l generate-bash-completion
complete -c keybase -f -n "$ends keybase" -l gpg
complete -c keybase -f -n "$ends keybase" -l gpg-options
complete -c keybase -f -n "$ends keybase" -l gpgdir
complete -c keybase -f -n "$ends keybase" -l gui-config-file
complete -c keybase -f -n "$ends keybase" -l help -s h
complete -c keybase -f -n "$ends keybase" -l home -s H
complete -c keybase -f -n "$ends keybase" -l leveldb-num-files
complete -c keybase -f -n "$ends keybase" -l local-rpc-debug-unsafe
complete -c keybase -f -n "$ends keybase" -l log-file
complete -c keybase -f -n "$ends keybase" -l log-format
complete -c keybase -f -n "$ends keybase" -l log-prefix
complete -c keybase -f -n "$ends keybase" -l merkle-kids
complete -c keybase -f -n "$ends keybase" -l no-auto-fork -s F
complete -c keybase -f -n "$ends keybase" -l no-debug
complete -c keybase -f -n "$ends keybase" -l paramproof-kit
complete -c keybase -f -n "$ends keybase" -l pgpdir
complete -c keybase -f -n "$ends keybase" -l pid-file
complete -c keybase -f -n "$ends keybase" -l pinentry
complete -c keybase -f -n "$ends keybase" -l proof-cache-size
complete -c keybase -f -n "$ends keybase" -l prove-bypass
complete -c keybase -f -n "$ends keybase" -l proxy
complete -c keybase -f -n "$ends keybase" -l proxy-type
complete -c keybase -f -n "$ends keybase" -l push-disabled
complete -c keybase -f -n "$ends keybase" -l push-save-interval
complete -c keybase -f -n "$ends keybase" -l push-server-uri
complete -c keybase -f -n "$ends keybase" -l pvl-kit
complete -c keybase -f -n "$ends keybase" -l read-deleted-sigchain
complete -c keybase -f -n "$ends keybase" -l remember-passphrase
complete -c keybase -f -n "$ends keybase" -l run-mode
complete -c keybase -f -n "$ends keybase" -l scraper-timeout
complete -c keybase -f -n "$ends keybase" -l secret-keyring
complete -c keybase -f -n "$ends keybase" -l server -s s
complete -c keybase -f -n "$ends keybase" -l session-file
complete -c keybase -f -n "$ends keybase" -l slow-gregor-conn
complete -c keybase -f -n "$ends keybase" -l socket-file
complete -c keybase -f -n "$ends keybase" -l standalone
complete -c keybase -f -n "$ends keybase" -l timers
complete -c keybase -f -n "$ends keybase" -l tor-hidden-address
complete -c keybase -f -n "$ends keybase" -l tor-mode
complete -c keybase -f -n "$ends keybase" -l tor-proxy
complete -c keybase -f -n "$ends keybase" -l updater-config-file
complete -c keybase -f -n "$ends keybase" -l use-default-log-file
complete -c keybase -f -n "$ends keybase" -l use-root-config-file
complete -c keybase -f -n "$ends keybase" -l user-cache-size
complete -c keybase -f -n "$ends keybase" -l vdebug
complete -c keybase -f -n "$ends keybase" -l version -s v

#commands
#L1
complete -c keybase -f -n "not $ends keybase"
complete -c keybase -f -n "$ends keybase" -a "$keybase"
#L2
complete -c keybase -f -n "$ends keybase account" -a "$keybase_account"
complete -c keybase -f -n "$ends keybase blocks" -a "$keybase_blocks"
complete -c keybase -f -n "$ends keybase bot" -a "$keybase_bot"
complete -c keybase -f -n "$ends keybase chat" -a "$keybase_chat"
complete -c keybase -f -n "$ends keybase config" -a "$keybase_config"
complete -c keybase -f -n "$ends keybase ctl" -a "$keybase_ctl"
complete -c keybase -f -n "$ends keybase currency" -a "$keybase_currency"
complete -c keybase -f -n "$ends keybase device" -a "$keybase_device"
complete -c keybase -f -n "$ends keybase fs" -a "$keybase_fs"
complete -c keybase -f -n "$ends keybase git" -a "$keybase_git"
complete -c keybase -f -n "$ends keybase h" -a "$keybase_help"
complete -c keybase -f -n "$ends keybase help" -a "$keybase_help"
complete -c keybase -f -n "$ends keybase kvstore" -a "$keybase_kvstore"
complete -c keybase -f -n "$ends keybase log" -a "$keybase_log"
complete -c keybase -f -n "$ends keybase passphrase" -a "$keybase_passphrase"
complete -c keybase -f -n "$ends keybase pgp" -a "$keybase_pgp"
complete -c keybase -f -n "$ends keybase rekey" -a "$keybase_rekey"
complete -c keybase -f -n "$ends keybase sigs" -a "$keybase_sigs"
complete -c keybase -f -n "$ends keybase team" -a "$keybase_team"
complete -c keybase -f -n "$ends keybase update" -a "$keybase_update"
complete -c keybase -f -n "$ends keybase wallet" -a "$keybase_wallet"
#...
#L3
complete -c keybase -f -n "$ends keybase account email" -a "$keybase_account_email"
complete -c keybase -f -n "$ends keybase fs debug" -a "$keybase_fs_debug"
complete -c keybase -f -n "$ends keybase fs sync" -a "$keybase_fs_sync"
#...

#command options
complete -c keybase -f -n "$ends keybase account upload-avatar" -l skip-chat-message -s s
complete -c keybase -f -n "$ends keybase account upload-avatar" -l team
#...
