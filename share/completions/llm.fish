# Completions for simonw/llm

set -l subcmds prompt aliases chat collections embed embed-models embed-multi install keys logs models openai plugins similar templates uninstall
function __fish_llm_subcmds
    printf "%s\t%s\n" prompt "Execute a prompt" \
        aliases "Manage model aliases" \
        chat "Hold chat with model" \
        collections "View/manage embedding collections" \
        embed "Embed text and get/store result" \
        embed-models "Manage available embedding models" \
        embed-multi "Store embeddings for multiple strings" \
        install "Install PyPI packages into llm env" \
        keys "Manage stored API keys" \
        logs "Explore logged prompts/responses" \
        models "Manage available models" \
        openai "Work with OpenAI API directly" \
        plugins "List installed plugins" \
        similar "Return top-N similar IDs for collection" \
        templates "Manage stored prompt templates" \
        uninstall "Uninstall Python packages from llm env"
end

complete -c llm -n __fish_is_first_token -xa "(__fish_llm_subcmds)"

# This applies to the base command only
complete -c llm -n "not __fish_seen_subcommand_from $subcmds" -l version -d "Show version info"
# This applies to the base command or any subcommands
complete -c llm -l help -d "Show command usage info" -x

function __fish_llm_models
    llm models |
        string replace -r '^[^:\\n]+: (\\S+)(?: \\(aliases: )?((?:[^),\\s]+,?)?+.*?)\\)?$' '$1 $2' |
        string split ' ' -n |
        string trim -c ','
end

function __fish_embedding_models
    llm models |
        string replace -r '^[^:\\n]+: (\\S+)(?: \\(aliases: )?((?:[^),\\s]+,?)?+.*?)\\)?$' '$1 $2' |
        string split ' ' -n |
        string trim -c ','
end

# The default subcommand is 'prompt'
set -l condition "not __fish_seen_subcommand_from $subcmds || __fish_seen_subcommand_from prompt"
complete -c llm -n $condition -s s -l system -d "System prompt to use" -r
complete -c llm -n $condition -s m -l model -d "Model to use" -xa "(__fish_llm_models)"
complete -c llm -n $condition -s a -l attachment -d "Attachment to use" -ra-
complete -c llm -n $condition -l at -d "Attachment type" -r
complete -c llm -n $condition -l attachment-type -d "Attachment type" -r
complete -c llm -n $condition -s n -l no-log -d "Don't log to db" -x
complete -c llm -n $condition -s l -l log -d "Log prompt/reply to db" -x
complete -c llm -n $condition -s c -l continue -d "Continue most recent conversation" -x
complete -c llm -n $condition -l key -d "API key to use" -r
complete -c llm -n $condition -l save -d "Save prompt as template with name" -x

# llm aliases
set -l condition "__fish_seen_subcommand_from aliases"
complete -c llm -n $condition -n '__fish_is_nth_token 2' -xa list -d "List current aliases" -x
complete -c llm -n $condition -n '__fish_is_nth_token 2' -xa path -d "Print path of llm's aliases.json" -x
complete -c llm -n $condition -n '__fish_is_nth_token 2' -xa remove -d "Remove an llm alias" -r
complete -c llm -n $condition -n '__fish_is_nth_token 2' -xa set -d "Set an alias for a model" -r
complete -c llm -n $condition -n '__fish_is_nth_token 4' -xa "(__fish_llm_models)" -d "Alias target model"

# llm chat
set -l condition "__fish_seen_subcommand_from chat"
complete -c llm -n $condition -s s -l system -d "System prompt to use" -r
complete -c llm -n $condition -s m -l model -d "Model to use" -xa "(__fish_llm_models)"
complete -c llm -n $condition -l cid -d "Continue conversation with given id" -x
complete -c llm -n $condition -l conversation -d "Continue conversation with given id" -x
complete -c llm -n $condition -s t -l template -d "Template to use" -x
complete -c llm -n $condition -s p -l param -d "Set template parameter to value" -x
complete -c llm -n $condition -s o -l option -d "Set key/value option for model" -x
complete -c llm -n $condition -l no-stream -d "Do not stream output" -x
complete -c llm -n $condition -l key -d "API key to use" -x

# llm collections
set -l condition "__fish_seen_subcommand_from collections"
complete -c llm -n $condition -xa list -d "List collections" -x
complete -c llm -n $condition -xa delete -d "Delete specified collection" -x
complete -c llm -n $condition -xa path -d "Print path to embeddings database" -x

# llm embed
set -l condition "__fish_seen_subcommand_from embed"
complete -c llm -n $condition -s i -l input -d "File to embed" -r
complete -c llm -n $condition -s m -l model -d "Model to use" -xa "(__fish_embedding_models)"
complete -c llm -n $condition -l store -d "Store the text itself in the db" -x
complete -c llm -n $condition -s d -l database -d "Path to db to use" -r
complete -c llm -n $condition -s c -l content -d "Text content to embed" -x
complete -c llm -n $condition -l binary -d "Treat input as binary" -x
complete -c llm -n $condition -l metadata -d "JSON object metadata to store" -x
complete -c llm -n $condition -s f -l format -d "Output format" -xa "json blob base64 hex"

# llm embed-models
set -l condition "__fish_seen_subcommand_from embed-models"
complete -c llm -n $condition -xa list -d "List available embedding models" -x
complete -c llm -n $condition -xa default -d "Show or set default embedding model" -x

# llm embed-multi
set -l condition "__fish_seen_subcommand_from embed-multi"
complete -c llm -n $condition -l format -xa "json csv tsv nl" -d "Format of input (default: auto-detected)"
complete -c llm -n $condition -l files -r -d "Embed files in DIR matching GLOB"
complete -c llm -n $condition -l encoding -r -d "Encoding to use when reading input"
complete -c llm -n $condition -l binary -d "Treat input as binary"
complete -c llm -n $condition -l sql -x -d "Read input using this SQL query"
complete -c llm -n $condition -l attach -x -d "Attach db ALIAS from PATH"
complete -c llm -n $condition -l batch-size -x -d "Batch size to use for embeddings"
complete -c llm -n $condition -l prefix -x -d "Prefix to add to the IDs"
complete -c llm -n $condition -s m -l model -d "Embedding model to use" -xa "(__fish_embedding_models)"
complete -c llm -n $condition -l store -d "Store the text itself in the db"
complete -c llm -n $condition -s d -l database -d "Path to db to use"

# llm install
set -l condition "__fish_seen_subcommand_from install"
complete -c llm -n $condition -s U -l upgrade -d "Upgrade packages to latest version" -x
complete -c llm -n $condition -s e -l editable -d "Install project in editable mode from PATH" -r
complete -c llm -n $condition -l force-reinstall -d "Reinstall all packages, even if up-to-date" -x
complete -c llm -n $condition -l no-cache-dir -d "Disable cache" -x

# llm keys
set -l condition "__fish_seen_subcommand_from keys"
complete -c llm -n $condition -xa list -d "List names of all stored keys"
complete -c llm -n $condition -xa get -d "Print saved key"
complete -c llm -n $condition -xa path -d "Print path of llm's keys.json"
complete -c llm -n $condition -xa set -d "Save a key in llm's keys.json"

# llm logs
set -l condition "__fish_seen_subcommand_from logs"
complete -c llm -n $condition -xa list -d "List recent prompts and responses"
complete -c llm -n $condition -xa off -d "Turn off logging"
complete -c llm -n $condition -xa on -d "Turn on logging"
complete -c llm -n $condition -xa path -d "Print path to llm's logs.db"
complete -c llm -n $condition -xa status -d "Show current status of db logging"

# llm models
set -l condition "__fish_seen_subcommand_from models"
complete -c llm -n $condition -xa list -d "List available models" -x
complete -c llm -n $condition -xa default -d "Show or set default model" -x

# llm plugins
set -l condition "__fish_seen_subcommand_from plugins"
complete -c llm -n $condition -l all -d "Include built-in/default plugins" -x
