#Keybase 5.0.0

#variables
set -l seen __fish_seen_subcommand_from
set -l commands account blocks bot chat ctl currency decrypt deprovision device encrypt follow fs git help id list-followers list-following log login logout oneshot paperkey passphrase pgp ping prove rekey selfprovision service sign signup sigs status team track unfollow untrack update verify version wallet

#global options
#...

#commands
complete -c keybase -f -n "not $seen $commands" -a "$commands"
#complete -c keybase -f -n "not $seen $commands" -a account -d "Modify your account"
#complete -c keybase -f -n "not $seen $commands" -a blocks -d "Manage user and team blocks"
#complete -c keybase -f -n "not $seen $commands" -a bot -d "Manage bot accounts"
#complete -c keybase -f -n "not $seen $commands" -a chat -d "Chat securely with keybase users"
#complete -c keybase -f -n "not $seen $commands" -a ctl -d "Control the background keybase service"
#complete -c keybase -f -n "not $seen $commands" -a currency -d "Manage cryptocurrency addresses"
#complete -c keybase -f -n "not $seen $commands" -a decrypt -d "Decrypt messages or files for keybase users"
#complete -c keybase -f -n "not $seen $commands" -a deprovision -d "Revoke the current device and log out"
#complete -c keybase -f -n "not $seen $commands" -a device -d "Manage your devices"
#...

#command options
#...

#USAGE:
#   keybase [global options] command [command options] [arguments...]
#   
#VERSION:
#   5.0.0-20191114203213+f73f97dac6
#   
#COMMANDS:
#   account		Modify your account
#   blocks		Manage user and team blocks
#   bot			Manage bot accounts
#   chat			Chat securely with keybase users
#   ctl			Control the background keybase service
#   currency		Manage cryptocurrency addresses
#   decrypt		Decrypt messages or files for keybase users
#   deprovision		Revoke the current device, log out, and delete local state.
#   device		Manage your devices
#   encrypt		Encrypt messages or files for keybase users and teams
#   follow, track	Verify a user's authenticity and optionally follow them
#   fs			Perform filesystem operations
#   git			Manage git repos
#   id			Identify a user and check their signature chain
#   list-followers	List those who follow you
#   list-following	List who you or the given user is publicly following
#   log			Manage keybase logs
#   login		Establish a session with the keybase server
#   logout		Logout and remove session information
#   oneshot		Establish a oneshot device, as in logging into keybase from a disposable docker
#   paperkey		Generate paper keys for recovering your account
#   passphrase		Change or recover your keybase passphrase
#   pgp			Manage keybase PGP keys
#   ping			Ping the keybase API server
#   prove		Generate a new proof
#   rekey		Rekey status and actions
#   selfprovision	Provision a new device if the current device is a clone.
#   sign			Sign a document
#   signup		Signup for a new account
#   sigs			Manage signatures
#   status		Show information about current user
#   team			Manage teams
#   unfollow, untrack	Unfollow a user
#   update		The updater
#   verify		Verify message or file signatures for keybase users
#   version		Print out version and build information
#   wallet		Send and receive Stellar XLM
#   service		start the Keybase service to power all other CLI options
#   help, h		Shows a list of commands or help for one command
#
#ADDITIONAL HELP TOPICS:
#   advanced		Description of advanced global options
#   gpg			Description of how keybase interacts with GPG
#   keyring		Description of how keybase stores secret keys locally
#   tor			Description of how keybase works with Tor
