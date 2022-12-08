set -l _sub_commands 'login login_cli activate logout auth tui whoami whois notifications instance search thread timeline post upload delete favourite unfavourite reblog unreblog reblogged_by pin unpin follow unfollow mute unmute block unblock'

function __fish_complete_toot_accounts
    toot auth | awk '/^\*/{print $2}'
end

# No file completion for most subcomands
complete -c toot -f

# # Subcommands
complete -c toot -a activate -n __fish_is_first_token -d 'Switch between accounts.'
complete -c toot -a auth -n __fish_is_first_token -d 'Show logged in accounts and instances'
complete -c toot -a instance -n __fish_is_first_token -d 'Display instance details'
complete -c toot -a login -n __fish_is_first_token -d 'Log in using your browser'
complete -c toot -a login_cli -n __fish_is_first_token -d 'Log in from the console'
complete -c toot -a logout -n __fish_is_first_token -d 'Log out'
complete -c toot -a notifications -n __fish_is_first_token -d 'Show notifications'
complete -c toot -a post -n __fish_is_first_token -d 'Post a status text'
complete -c toot -a search -n __fish_is_first_token -d 'Search for users or hashtags'
complete -c toot -a thread -n __fish_is_first_token -d 'Show toot thread items'
complete -c toot -a timeline -n __fish_is_first_token -d 'Show recent items'
complete -c toot -a tui -n __fish_is_first_token -d 'Toot terminal user interface'
complete -c toot -a upload -n __fish_is_first_token -d 'Upload a file'
complete -c toot -a whoami -n __fish_is_first_token -d 'Display user details'
complete -c toot -a whois -n __fish_is_first_token -d 'Display account details'
complete -c toot -a delete -n __fish_is_first_token -d 'Delete a status'
complete -c toot -a favourite -n __fish_is_first_token -d 'Favourite a status'
complete -c toot -a pin -n __fish_is_first_token -d 'Pin a status'
complete -c toot -a reblog -n __fish_is_first_token -d 'Reblog a status'
complete -c toot -a reblogged_by -n __fish_is_first_token -d 'Show rebloggers of status'
complete -c toot -a unfavourite -n __fish_is_first_token -d 'Unfavourite a status'
complete -c toot -a unpin -n __fish_is_first_token -d 'Unpin a status'
complete -c toot -a unreblog -n __fish_is_first_token -d 'Unreblog a status'

# Accounts
complete -c toot -a follow -n __fish_is_first_token -d 'Follow an account'
complete -c toot -a unfollow -n __fish_is_first_token -d 'Unfollow an account'
complete -c toot -a mute -n __fish_is_first_token -d 'Mute an account'
complete -c toot -a unmute -n __fish_is_first_token -d 'Unmute an account'
complete -c toot -a block -n __fish_is_first_token -d 'Block an account'
complete -c toot -a unblock -n __fish_is_first_token -d 'Unblock an account'

# Parameters common to all subcommands
complete -c toot -n "__fish_seen_subcommand_from $_sub_commands" -l debug -d 'show debug log'
complete -c toot -n "__fish_seen_subcommand_from $_sub_commands" -l no-color -d "no colors in output"
complete -c toot -n "__fish_seen_subcommand_from $_sub_commands" -l quiet -d "don't write to stdout on success"
complete -c toot -n "__fish_seen_subcommand_from $_sub_commands" -s h -l help -d 'show help message'

# Parameters common to some subcommands
complete -c toot -n '__fish_seen_subcommand_from activate logout' -a "(__fish_complete_toot_accounts)"
complete -c toot -n '__fish_seen_subcommand_from login login_cli instance' -l disable-https -d 'disable HTTPS'
complete -c toot -n '__fish_seen_subcommand_from login login_cli timeline' -s i -l instance -x -d 'mastodon instance'
complete -c toot -n '__fish_seen_subcommand_from tui whoami whois notifications search thread timeline post upload delete favourite unfavourite reblog unreblog pin unpin follow unfollow mute unmute block unblock' -s u -l using -x -a "(__fish_complete_toot_accounts)" -d 'account to use'
complete -c toot -n '__fish_seen_subcommand_from notifications timeline' -s r -l reverse -x -d 'reverse order of output'
complete -c toot -n '__fish_seen_subcommand_from post upload' -s d -l description -x -d 'alt text for media file'

# Parameters for login_cli
complete -c toot -n '__fish_seen_subcommand_from login_cli' -s e -l email -x -d 'email address'

# Parameters for notifications
complete -c toot -n '__fish_seen_subcommand_from notifications' -l clear -d 'delete all notifications'
complete -c toot -n '__fish_seen_subcommand_from notifications' -s m -l mentions -x -d 'only print mentions'

# Parameters for search
complete -c toot -n '__fish_seen_subcommand_from search' -s r -l resolve -x -d 'resolve non-local accounts'

# Parameters for timeline
complete -c toot -n '__fish_seen_subcommand_from timeline' -s p -l public -d 'show public timeline'
complete -c toot -n '__fish_seen_subcommand_from timeline' -s t -l tag -x -d 'show hastag timeline'
complete -c toot -n '__fish_seen_subcommand_from timeline' -s l -l local -d 'show only statuses from local instance'
complete -c toot -n '__fish_seen_subcommand_from timeline' -l list -x -d 'show timeline for given list'
complete -c toot -n '__fish_seen_subcommand_from timeline' -s c -l count -x -d 'number of toots per page'
complete -c toot -n '__fish_seen_subcommand_from timeline' -s 1 -l once -d 'do not prompt to continue'

# Parameters for post
complete -c toot -n '__fish_seen_subcommand_from post' -s m -l media -r -F -d 'path to the media file'
complete -c toot -n '__fish_seen_subcommand_from post' -s v -l visibility -x -d 'post visibility' -a 'public unlisted private direct'
complete -c toot -n '__fish_seen_subcommand_from post' -s s -l sensitive -d 'mark as NSFW'
complete -c toot -n '__fish_seen_subcommand_from post' -s p -l spoiler-text -x -d 'warning text before content'
complete -c toot -n '__fish_seen_subcommand_from post' -s r -l reply-to -x -d 'ID of the status to reply to'
complete -c toot -n '__fish_seen_subcommand_from post' -s l -l language -x -d 'language code of the toot'
complete -c toot -n '__fish_seen_subcommand_from post' -s e -l editor -x -d 'editor to compose toot'
complete -c toot -n '__fish_seen_subcommand_from post' -l scheduled-at -x -d 'datetime at which to schedule'
complete -c toot -n '__fish_seen_subcommand_from post' -s t -l content-type -x -d 'MIME type'

# Parameters for upload
complete -c toot -n '__fish_seen_subcommand_from upload' -F
