# from https://github.com/zk-org/zk/blob/main/internal/cli/cmd/edit.go
set -l commands init index config new list graph edit tag

set -l zk_list_notes "(command zk list -qf '{{path}}\t{{title}}' 2>/dev/null)"
set -l zk_list_tags "(command zk tag -qf '{{name}}\t{{note-count}}' 2>/dev/null)"

complete -c zk -f
complete -c zk -n __fish_use_subcommand -a init -d "Create a new notebook in the given directory"
complete -c zk -n __fish_use_subcommand -a index -d "Index the notes to be searchable"
complete -c zk -n __fish_use_subcommand -a config -d "List configuration parameters"
complete -c zk -n __fish_use_subcommand -a new -d "Create a new note in the given notebook directory"
complete -c zk -n __fish_use_subcommand -a list -d "List notes matching the given criteria"
complete -c zk -n __fish_use_subcommand -a graph -d "Produce a graph of the notes matching the given criteria"
complete -c zk -n __fish_use_subcommand -a edit -d "Edit notes matching the given criteria"
complete -c zk -n __fish_use_subcommand -a tag -d "Manage the note tags"
complete -c zk -n "__fish_seen_subcommand_from index config tag" -f

# global flags
complete -c zk -l help -s h -d "show help".
complete -c zk -l notebook-dir -d 'Turn off notebook auto-discovery and set manually the notebook where commands are run.' -xa '(__fish_complete_directories)'
complete -c zk -l working-dir -s W -r -d 'Run as if zk was started in <PATH> instead of the current working directory.' -xa '(__fish_complete_directories)'
complete -c zk -l no-input -d 'Never prompt or ask for confirmation.'

# init
complete -c zk -n "__fish_seen_subcommand_from init" -fa "(__fish_complete_directories)"

# index
complete -c zk -n "__fish_seen_subcommand_from index" -l force -s f -d "Force indexing all the notes"
complete -c zk -n "__fish_seen_subcommand_from index" -l verbose -s v -d "Print detailed information about the indexing process"
complete -c zk -n "__fish_seen_subcommand_from index" -l quiet -s q -d "Do not print statistics nor progress"

# config
complete -c zk -n "__fish_seen_subcommand_from config" -l list -s l -x -a "aliases filters extras" -d "List configuration objects. Listable objects are: aliases, filters and extras"
complete -c zk -n "__fish_seen_subcommand_from config" -l format -s f -x -a 'short full json' -d "Pretty print the list using a custom template or predefined formats: short, full, json"
complete -c zk -n "__fish_seen_subcommand_from config" -l header -d "Arbitrary text printed at the start of the list"
complete -c zk -n "__fish_seen_subcommand_from config" -l footer -x -d "Arbitrary text printed at the end of the list"
complete -c zk -n "__fish_seen_subcommand_from config" -l delimiter -s d -x -d "Print tags delimited by the given separator"
complete -c zk -n "__fish_seen_subcommand_from config" -l delimiter0 -s 0 -d "Print tags delimited by ASCII NUL characters. Useful with xargs -0"
complete -c zk -n "__fish_seen_subcommand_from config" -l no-pager -s P -d "Do not pipe output into a pager"
complete -c zk -n "__fish_seen_subcommand_from config" -l quiet -s q -d "Do not print the total number of tags found"

# new
complete -c zk -n "__fish_seen_subcommand_from new" -fa "(__fish_complete_directories)"
complete -c zk -n "__fish_seen_subcommand_from new" -l interactive -s i -d "Read contents from standard input"
complete -c zk -n "__fish_seen_subcommand_from new" -l title -s t -x -d "Title of the new note"
complete -c zk -n "__fish_seen_subcommand_from new" -l date -x -d "Set the current date"
complete -c zk -n "__fish_seen_subcommand_from new" -l group -s g -x -d "Name of the config group this note belongs to. Takes precedence over the config of the directory"
complete -c zk -n "__fish_seen_subcommand_from new" -l extra -x -d "Extra variables passed to the templates"
complete -c zk -n "__fish_seen_subcommand_from new" -l template -r -d "Custom template used to render the note"
complete -c zk -n "__fish_seen_subcommand_from new" -l print-path -s p -d "Print the path of the created note instead of editing it"
complete -c zk -n "__fish_seen_subcommand_from new" -l dry-run -s n -d "Don't actually create the note. Instead, prints its content on stdout and the generated path on stderr"
complete -c zk -n "__fish_seen_subcommand_from new" -l id -x -d "Skip id generation and use provided value"

# list
complete -c zk -n "__fish_seen_subcommand_from list" -fa "$zk_list_notes"
complete -c zk -n "__fish_seen_subcommand_from list" -l format -s f -xa "oneline short medium long full json jsonl" -d "Pretty print the list using a custom template or one of the predefined formats"
complete -c zk -n "__fish_seen_subcommand_from list" -l header -x -d "Arbitrary text printed at the start of the list"
complete -c zk -n "__fish_seen_subcommand_from list" -l footer -x -d "Arbitrary text printed at the end of the list"
complete -c zk -n "__fish_seen_subcommand_from list" -l delimiter -s d -x -d "Print notes delimited by the given separator"
complete -c zk -n "__fish_seen_subcommand_from list" -l delimiter0 -s 0 -d "Print notes delimited by ASCII NUL characters. This is useful when used in conjunction with 'xargs -0'"
complete -c zk -n "__fish_seen_subcommand_from list" -l no-pager -s P -d "Do not pipe output into a pager"
complete -c zk -n "__fish_seen_subcommand_from list" -l quiet -s q -d "Do not print the total number of notes found"

# graph
complete -c zk -n "__fish_seen_subcommand_from graph" -fa "$zk_list_notes"
complete -c zk -n "__fish_seen_subcommand_from graph" -l format -s f -xa json -d "Format of the graph"
complete -c zk -n "__fish_seen_subcommand_from graph" -l quiet -s q -d "Do not print the total number of notes found"

# edit
complete -c zk -n "__fish_seen_subcommand_from edit" -fa "$zk_list_notes"
complete -c zk -n "__fish_seen_subcommand_from edit" -l force -s f -d "Do not confirm before editing many notes at the same time"

# tag
complete -c zk -n "__fish_seen_subcommand_from tag" -l format -s f -xa "name full json jsonl" -d "Pretty print the list using a custom template or one of the predefined formats"
complete -c zk -n "__fish_seen_subcommand_from tag" -l header -x -d "Arbitrary text printed at the start of the list"
complete -c zk -n "__fish_seen_subcommand_from tag" -l footer -x -d "Arbitrary text printed at the end of the list"
complete -c zk -n "__fish_seen_subcommand_from tag" -l delimiter -s d -x -d "Print tags delimited by the given separator"
complete -c zk -n "__fish_seen_subcommand_from tag" -l delimiter0 -s 0 -d "Print tags delimited by ASCII NUL characters. This is useful when used in conjunction with 'xargs -0'"
complete -c zk -n "__fish_seen_subcommand_from tag" -l no-pager -s P -d "Do not pipe output into a pager"
complete -c zk -n "__fish_seen_subcommand_from tag" -l quiet -s q -d "Do not print the tags number of notes found"
complete -c zk -n "__fish_seen_subcommand_from tag" -l sort -s s -x -d "Order the tags by the given criterion"

# filtering
# https://github.com/zk-org/zk/blob/main/internal/cli/filtering.go
set -l filter_commands list graph edit
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l interactive -s i -d "Select notes interactively with fzf"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l limit -s n -x -d "Limit the number of notes found"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l match -s m -x -d "Terms to search for in the notes"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l match-strategy -s M -x -d "Text matching strategy among: fts, re, exact"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l exclude -s x -xa "$zk_list_notes" -d "Ignore notes matching the given path, including its descendants"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l tag -s t -xa "$zk_list_tags" -d "Find notes tagged with the given tags"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l mention -xa "$zk_list_notes" -d "Find notes mentioning the title of the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l mentioned-by -xa "$zk_list_notes" -d "Find notes whose title is mentioned in the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l link-to -s l -xa "$zk_list_notes" -d "Find notes which are linking to the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l no-link-to -xa "$zk_list_notes" -d "Find notes which are not linking to the given notes"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l linked-by -s L -xa "$zk_list_notes" -d "Find notes which are linked by the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l no-linked-by -xa "$zk_list_notes" -d "Find notes which are not linked by the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l orphan -d "Find notes which are not linked by any other note"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l tagless -d "Find notes which have no tags"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l missing-backlink -d "Find notes with at least one missing backlink"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l broken-links -d "Find notes with at least one broken internal link"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l related -xa "$zk_list_notes" -d "Find notes which might be related to the given ones"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l max-distance -x -d "Maximum distance between two linked notes"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l recursive -s r -d "Follow links recursively"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l created -x -d "Find notes created on the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l created-before -x -d "Find notes created before the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l created-after -x -d "Find notes created after the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l modified -x -d "Find notes modified on the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l modified-before -x -d "Find notes modified before the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l modified-after -x -d "Find notes modified after the given date"
complete -c zk -n "__fish_seen_subcommand_from $filter_commands" -l sort -s s -x -d "Order the notes by the given criterion"
