# Keybase 6.4.0

function __fish_keybase_line_ends_with
    set -l line (commandline -pxc | string match -v -r '^-')
    for i in (seq -1 -1 -(count $argv))
        if test "$line[$i]" != "$argv[$i]"
            return 1
        end
    end
end

# variables
set -l ends __fish_keybase_line_ends_with
# 1
set -l keybase account apicall audit base62 blocks bot btc ca chat config ctl currency db decrypt deprovision device dir dismiss dismiss-category dump-keyfamily dump-push-notifications encrypt fnmr follow fs git h help home id interesting-people kvstore list-followers list-following log login logout network-stats oneshot paperkey passphrase pgp ping pprof prove push rekey riit script selfprovision service sign signup sigs status team track unfollow unlock untrack upak update upload-avatar verify version wallet web-auth-token whoami wot
# 2
set -l keybase_account contact-settings delete email h help lockdown phonenumber recover-username reset-cancel upload-avatar
set -l keybase_audit box h help
set -l keybase_base62 decode encode h help
set -l keybase_blocks h help list-teams list-users
set -l keybase_bot h help signup token
set -l keybase_chat add-bot-member add-to-channel api api-listen archive archive-delete archive-list archive-pause archive-resume bot-member-settings clear-commands conv-info create-channel default-channels delete-channel delete-history download edit-bot-member emoji-add emoji-addalias emoji-list emoji-remove featured-bots forward-message h help hide join-channel leave-channel list list-channels list-members list-unread ls lsur mark-as-read min-writer-role mute notification-settings read readd-member remove-bot-member remove-from-channel rename-channel report retention-policy search search-bots search-regexp send upload
set -l keybase_config get h help info set
set -l keybase_ctl app-exit autostart h help init log-rotate redirector reload restart start stop wait wants-systemd watchdog watchdog2
set -l keybase_currency add h help
set -l keybase_db clean delete get h help keys-with-prefix nuke put
set -l keybase_device add h help list remove
set -l keybase_fs archive cancel-uploads clear-conflicts cp debug finish-resolving-conflicts get-status h help history index-progress kill ln ls mkdir mv ps quota read recover reset reset-index rm search set-debug-level stat sync uploads write
set -l keybase_git archive create delete gc h help lfs-config list settings
set -l keybase_help advanced gpg keyring tor
set -l keybase_kvstore api h help
set -l keybase_log h help profile send
set -l keybase_passphrase change check h help recover remember set
set -l keybase_pgp decrypt drop encrypt export gen h help import list pull pull-private purge push-private select sign update verify
set -l keybase_pprof cpu h heap help trace
set -l keybase_rekey h help paper status
set -l keybase_sigs h help list revoke
set -l keybase_team accept-invite add-member add-members-bulk api bot-settings create delete edit-member ftl generate-invite-token generate-invitelink h help ignore-request leave list-members list-memberships list-requests profile-load remove-member rename request-access search settings show-tree
set -l keybase_update check check-in-use notify
set -l keybase_wallet accounts balances detail details export h help list
set -l keybase_wot accept h help list reject vouch
# 3
set -l keybase_account_email add delete edit h help list send-verification-email set-primary set-visibility
set -l keybase_account_phonenumber add delete edit h help list set-visibility verify
set -l keybase_bot_token create delete h help list
set -l keybase_fs_archive cancel check check dismiss h help start status
set -l keybase_fs_debug deobfuscate dump h help obfuscate
set -l keybase_fs_sync disable enable h help show

# global options
complete -c keybase -f -n "$ends keybase" -l api-dump-unsafe -d "Dump API call internals (may leak secrets)"
complete -c keybase -f -n "$ends keybase" -l api-timeout -d "Set the HTTP timeout for API calls to the keybase API server"
complete -c keybase -f -n "$ends keybase" -l api-uri-path-prefix -d "Specify an alternate API URI path prefix"
complete -c keybase -f -n "$ends keybase" -l app-start-mode -d "Specify 'service' to auto-start UI app, or anything else to disable"
complete -c keybase -f -n "$ends keybase" -l auto-fork -d "Enable auto-fork of background service"
complete -c keybase -f -n "$ends keybase" -l bg-identifier-disabled -d "Supply to disable the BG identifier loop"
complete -c keybase -f -n "$ends keybase" -l chat-db -d "Specify an alternate local Chat DB location"
complete -c keybase -f -n "$ends keybase" -l code-signing-kids -d "Set of code signing key IDs (colon-separated)"
complete -c keybase -f -n "$ends keybase" -l config-file -s c -d "Specify an (alternate) master config file"
complete -c keybase -f -n "$ends keybase" -l db -d "Specify an alternate local DB location"
complete -c keybase -f -n "$ends keybase" -l debug -s d -d "Enable debugging mode"
complete -c keybase -f -n "$ends keybase" -l debug-journeycard -d "Enable experimental journey cards"
complete -c keybase -f -n "$ends keybase" -l disable-bg-conv-loader -d "Disable background conversation loading"
complete -c keybase -f -n "$ends keybase" -l disable-cert-pinning -d "WARNING: Do not use unless necessary!"
complete -c keybase -f -n "$ends keybase" -l disable-merkle-auditor -d "Disable background probabilistic merkle audit"
complete -c keybase -f -n "$ends keybase" -l disable-search-indexer -d "Disable chat search background indexer"
complete -c keybase -f -n "$ends keybase" -l disable-team-auditor -d "Disable auditing of teams"
complete -c keybase -f -n "$ends keybase" -l disable-team-box-auditor -d "Disable box auditing of teams"
complete -c keybase -f -n "$ends keybase" -l display-raw-untrusted-output -d "Display output from users without escaping terminal codes"
complete -c keybase -f -n "$ends keybase" -l ek-log-file -d "Specify a log file for the keybase ephemeral key log"
complete -c keybase -f -n "$ends keybase" -l enable-bot-lite-mode -d "Enable bot lite mode. Disables non-critical background services"
complete -c keybase -f -n "$ends keybase" -l extra-net-logging -d "Do additional debug logging during network requests"
complete -c keybase -f -n "$ends keybase" -l features -d "Specify experimental feature flags"
complete -c keybase -f -n "$ends keybase" -l force-linux-keyring -d "Require the use of the OS keyring"
complete -c keybase -f -n "$ends keybase" -l generate-bash-completion -d ""
complete -c keybase -f -n "$ends keybase" -l gpg -d "Path to GPG client (optional for exporting keys)"
complete -c keybase -f -n "$ends keybase" -l gpg-options -d "Options to use when calling GPG"
complete -c keybase -f -n "$ends keybase" -l gpgdir -d "Specify a PGP directory (default is ~/.gnupg)"
complete -c keybase -f -n "$ends keybase" -l gui-config-file -d "Specify a path to the GUI config file"
complete -c keybase -f -n "$ends keybase" -l help -s h -d "Show help"
complete -c keybase -f -n "$ends keybase" -l home -s H -d "Specify an (alternate) home directory"
complete -c keybase -f -n "$ends keybase" -l leveldb-num-files -d "Specify the max number of files LevelDB may open"
complete -c keybase -f -n "$ends keybase" -l local-rpc-debug-unsafe -d "Use to debug local RPC (may leak secrets)"
complete -c keybase -f -n "$ends keybase" -l log-file -d "Specify a log file for the keybase service"
complete -c keybase -f -n "$ends keybase" -l log-format -d "Log format (default, plain, file, fancy)"
complete -c keybase -f -n "$ends keybase" -l log-prefix -d "Specify a prefix for a unique log file name"
complete -c keybase -f -n "$ends keybase" -l merkle-kids -d "Set of admissible Merkle Tree fingerprints (colon-separated)"
complete -c keybase -f -n "$ends keybase" -l no-auto-fork -s F -d "Disable auto-fork of background service"
complete -c keybase -f -n "$ends keybase" -l no-debug -d "Suppress debugging mode; takes precedence over --debug"
complete -c keybase -f -n "$ends keybase" -l paramproof-kit -d "Specify an alternate local parameterized proof kit file location"
complete -c keybase -f -n "$ends keybase" -l pgpdir -d "Specify a PGP directory (default is ~/.gnupg)"
complete -c keybase -f -n "$ends keybase" -l pid-file -d "Location of the keybased pid-file (to ensure only one running daemon)"
complete -c keybase -f -n "$ends keybase" -l pinentry -d "Specify a path to find a pinentry program"
complete -c keybase -f -n "$ends keybase" -l proof-cache-size -d "Number of proof entries to cache"
complete -c keybase -f -n "$ends keybase" -l prove-bypass -d "Prove even disabled proof services"
complete -c keybase -f -n "$ends keybase" -l proxy -d "Specify a proxy to ship all Web requests over"
complete -c keybase -f -n "$ends keybase" -l proxy-type -d "Set the proxy type; One of: socks,http_connect"
complete -c keybase -f -n "$ends keybase" -l push-disabled -d "Disable push server connection (which is on by default)"
complete -c keybase -f -n "$ends keybase" -l push-save-interval -d "Set the interval between saves of the push cache (in seconds)"
complete -c keybase -f -n "$ends keybase" -l push-server-uri -d "Specify a URI for contacting the Keybase push server"
complete -c keybase -f -n "$ends keybase" -l pvl-kit -d "Specify an alternate local PVL kit file location"
complete -c keybase -f -n "$ends keybase" -l read-deleted-sigchain -d "Allow admins to read deleted sigchains for debugging"
complete -c keybase -f -n "$ends keybase" -l remember-passphrase -d "Remember keybase passphrase"
complete -c keybase -f -n "$ends keybase" -l run-mode -d "Run mode (devel, staging, prod)"
complete -c keybase -f -n "$ends keybase" -l scraper-timeout -d "Set the HTTP timeout for external proof scrapers"
complete -c keybase -f -n "$ends keybase" -l secret-keyring -d "Location of the Keybase secret-keyring (P3SKB-encoded)"
complete -c keybase -f -n "$ends keybase" -l server -s s -d "Specify server API"
complete -c keybase -f -n "$ends keybase" -l session-file -d "Specify an alternate session data file"
complete -c keybase -f -n "$ends keybase" -l slow-gregor-conn -d "Slow responses from gregor for testing"
complete -c keybase -f -n "$ends keybase" -l socket-file -d "Location of the keybased socket-file"
complete -c keybase -f -n "$ends keybase" -l standalone -d "Use the client without any daemon support"
complete -c keybase -f -n "$ends keybase" -l timers -d "Specify 'a' for API; 'r' for RPCs; and 'x' for eXternal API calls"
complete -c keybase -f -n "$ends keybase" -l tor-hidden-address -d "Set TOR address of keybase server"
complete -c keybase -f -n "$ends keybase" -l tor-mode -d "Set TOR mode to be 'leaky', 'none', or 'strict'"
complete -c keybase -f -n "$ends keybase" -l tor-proxy -d "Set TOR proxy; when Tor mode is on; defaults to localhost:9050"
complete -c keybase -f -n "$ends keybase" -l updater-config-file -d "Specify a path to the updater config file"
complete -c keybase -f -n "$ends keybase" -l use-default-log-file -d "Log to the default log file in $XDG_CACHE_HOME, or ~/.cache if unset"
complete -c keybase -f -n "$ends keybase" -l use-root-config-file -d "Use the default root config on Linux only"
complete -c keybase -f -n "$ends keybase" -l user-cache-size -d "Number of User entries to cache"
complete -c keybase -f -n "$ends keybase" -l vdebug -d "Verbose debugging; takes a comma-joined list of levels and tags"
complete -c keybase -f -n "$ends keybase" -l version -s v -d "Print the version"

# commands
# 1
complete -c keybase -f -n "not $ends keybase"
complete -c keybase -f -n "$ends keybase" -a "$keybase"
# 2
complete -c keybase -f -n "$ends keybase account" -a "$keybase_account"
complete -c keybase -f -n "$ends keybase audit" -a "$keybase_audit"
complete -c keybase -f -n "$ends keybase base62" -a "$keybase_base62"
complete -c keybase -f -n "$ends keybase blocks" -a "$keybase_blocks"
complete -c keybase -f -n "$ends keybase bot" -a "$keybase_bot"
complete -c keybase -f -n "$ends keybase chat" -a "$keybase_chat"
complete -c keybase -f -n "$ends keybase config" -a "$keybase_config"
complete -c keybase -f -n "$ends keybase ctl" -a "$keybase_ctl"
complete -c keybase -f -n "$ends keybase currency" -a "$keybase_currency"
complete -c keybase -f -n "$ends keybase db" -a "$keybase_db"
complete -c keybase -f -n "$ends keybase device" -a "$keybase_device"
complete -c keybase -f -n "$ends keybase fs" -a "$keybase_fs"
complete -c keybase -f -n "$ends keybase git" -a "$keybase_git"
complete -c keybase -f -n "$ends keybase h" -a "$keybase_help"
complete -c keybase -f -n "$ends keybase help" -a "$keybase_help"
complete -c keybase -f -n "$ends keybase kvstore" -a "$keybase_kvstore"
complete -c keybase -f -n "$ends keybase log" -a "$keybase_log"
complete -c keybase -f -n "$ends keybase passphrase" -a "$keybase_passphrase"
complete -c keybase -f -n "$ends keybase pgp" -a "$keybase_pgp"
complete -c keybase -f -n "$ends keybase pprof" -a "$keybase_pprof"
complete -c keybase -f -n "$ends keybase rekey" -a "$keybase_rekey"
complete -c keybase -f -n "$ends keybase sigs" -a "$keybase_sigs"
complete -c keybase -f -n "$ends keybase team" -a "$keybase_team"
complete -c keybase -f -n "$ends keybase update" -a "$keybase_update"
complete -c keybase -f -n "$ends keybase wallet" -a "$keybase_wallet"
complete -c keybase -f -n "$ends keybase wot" -a "$keybase_wot"
# 3
complete -c keybase -f -n "$ends keybase account email" -a "$keybase_account_email"
complete -c keybase -f -n "$ends keybase account phonenumber" -a "$keybase_account_phonenumber"
complete -c keybase -f -n "$ends keybase bot token" -a "$keybase_bot_token"
complete -c keybase -f -n "$ends keybase fs archive" -a "$keybase_fs_archive"
complete -c keybase -f -n "$ends keybase fs debug" -a "$keybase_fs_debug"
complete -c keybase -f -n "$ends keybase fs sync" -a "$keybase_fs_sync"

# command options
complete -c keybase -f -n "$ends keybase account upload-avatar" -l skip-chat-message -s s
complete -c keybase -f -n "$ends keybase account upload-avatar" -l team
# ...
