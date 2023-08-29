# Fish completion for gnome-extensions

function __fish_gnome-extensions_complete_all_extensions
    gnome-extensions list
end

function __fish_gnome-extensions_complete_enabled_extensions
    gnome-extensions list --enabled
end

function __fish_gnome-extensions_complete_disabled_extensions
    gnome-extensions list --disabled
end

function __fish_gnome-extensions_complete_enabled_extensions_with_preferences
    gnome-extensions list --enabled --prefs
end

function __fish_gnome-extensions_complete_disabled_extensions_with_preferences
    gnome-extensions list --disabled --prefs
end

set -l commands_with_quiet enable disable reset uninstall list info show prefs create pack install
set -l commands help version $commands_with_quiet
set -l commands_without_help version $commands_with_quiet

complete -f -c gnome-extensions

complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a help -d "Print help"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a version -d "Print version"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a enable -d "Enable extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a disable -d "Disable extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a reset -d "Reset extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a uninstall -d "Uninstall extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a list -d "List extensions"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a "info show" -d "Show extension info"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a prefs -d "Open extension preferences"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a create -d "Create extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a pack -d "Package extension"
complete -c gnome-extensions -n "not __fish_seen_subcommand_from $commands" -a install -d "Install extension bundle"

complete -c gnome-extensions -n "__fish_seen_subcommand_from help && not __fish_seen_subcommand_from $commands_without_help" -a "$commands"
complete -c gnome-extensions -n "__fish_seen_subcommand_from enable && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_disabled_extensions)" -a "(__fish_gnome-extensions_complete_disabled_extensions)"
complete -c gnome-extensions -n "__fish_seen_subcommand_from disable && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_enabled_extensions)" -a "(__fish_gnome-extensions_complete_enabled_extensions)"
complete -c gnome-extensions -n "__fish_seen_subcommand_from reset && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_all_extensions)" -a "(__fish_gnome-extensions_complete_all_extensions)"
complete -c gnome-extensions -n "__fish_seen_subcommand_from uninstall && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_all_extensions)" -a "(__fish_gnome-extensions_complete_all_extensions)"

complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l user -d "Show user-installed extensions"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l system -d "Show system-installed extensions"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l enabled -d "Show enabled extensions"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l disabled -d "Show disabled extensions"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l prefs -d "Show extensions with preferences"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -l updates -d "Show extensions with updates"
complete -c gnome-extensions -n "__fish_seen_subcommand_from list" -s d -l details -d "Print extension details"

complete -c gnome-extensions -n "__fish_seen_subcommand_from info show && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_enabled_extensions)" -a "(__fish_gnome-extensions_complete_enabled_extensions)" -d Enabled
complete -c gnome-extensions -n "__fish_seen_subcommand_from info show && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_disabled_extensions)" -a "(__fish_gnome-extensions_complete_disabled_extensions)" -d Disabled

complete --keep-order -c gnome-extensions -n "__fish_seen_subcommand_from prefs && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_disabled_extensions_with_preferences) (__fish_gnome-extensions_complete_enabled_extensions_with_preferences)" -a "(__fish_gnome-extensions_complete_disabled_extensions_with_preferences)" -d Disabled
complete --keep-order -c gnome-extensions -n "__fish_seen_subcommand_from prefs && not __fish_seen_subcommand_from (__fish_gnome-extensions_complete_disabled_extensions_with_preferences) (__fish_gnome-extensions_complete_enabled_extensions_with_preferences)" -a "(__fish_gnome-extensions_complete_enabled_extensions_with_preferences)" -d Enabled

complete -x -c gnome-extensions -n "__fish_seen_subcommand_from create && not __fish_seen_argument -l uuid" -l uuid -d "The unique identifier of the new extension"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from create && not __fish_seen_argument -l name" -l name -d "The user-visible name of the new extension"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from create && not __fish_seen_argument -l description" -l description -d "A short description of what the extension does"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from create && not __fish_seen_argument -l template" -l template -d "The template to use for the new extension"
complete -c gnome-extensions -n "__fish_seen_subcommand_from create && not __fish_seen_argument -s i -l interactive" -s i -l interactive -d "Enter extension information interactively"

complete -x -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -l extra-source" -l extra-source -d "Additional source to include in the bundle"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -l schema" -l schema -d "A GSettings schema that should be included"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -l podir" -l podir -d "The directory where translations are found"
complete -x -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -l gettext-domain" -l gettext-domain -d "The gettext domain to use for translations"
complete -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -s f -l force" -s f -l force -d "Overwrite an existing pack"
complete -c gnome-extensions -n "__fish_seen_subcommand_from pack && not __fish_seen_argument -s o -l out-dir" -s o -l out-dir -d "The directory where the pack should be created"
complete -c gnome-extensions -n "__fish_seen_subcommand_from pack" -a "(__fish_complete_directories)" -d "Source directory"

complete -c gnome-extensions -n "__fish_seen_subcommand_from install && not __fish_seen_argument -s f" -s f -l force -d "Overwrite an existing extension"
complete -F -c gnome-extensions -n "__fish_seen_subcommand_from install" -d "Extension bundle"

complete -c gnome-extensions -n "__fish_seen_subcommand_from $commands_with_quiet" -s q -l quiet -d "Do not print error messages"
