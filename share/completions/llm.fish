# Basic completions for simonw/llm
# A complete implementation for `llm [prompt]` but other subcommands
# can be further fleshed out.

set -l subcmds prompt aliases chat collections embed embed-models embed-multi install keys logs models openai plugins similar templates uninstall
function __fish_llm_subcmds
    printf "%s\t%s\n" "prompt" "Execute a prompt" \
        "aliases" "Manage model aliases" \
        "chat" "Hold chat with model" \
        "collections" "View/manage embedding collections" \
        "embed" "Embed text and get/store result" \
        "embed-models" "Manage available embedding models" \
        "embed-multi\Store embeddings for multiple strings" \
        "install" "Install PyPI packages into llm env" \
        "keys" "Manage stored API keys" \
        "logs" "Explore logged prompts/responses" \
        "models" "Manage available models" \
        "openai" "Work with OpenAI API directly" \
        "plugins" "List installed plugins" \
        "similar" "Return top-N similar IDs for collection" \
        "templates" "Manage stored prompt templates" \
        "uninstall" "Uninstall Python packages from llm env"
end

complete -c llm -n __fish_is_first_token -xa "(__fish_llm_subcmds)"

# This applies to the base command only
complete -c llm -n "not __fish_seen_subcommand_from $subcmds" -l version -d "Show version info"
# This applies to the base command or any subcommands
complete -c llm -l help -d "Show usage info"

function __fish_llm_models
    llm models |
    string replace -r '^[^:]+: ([^ ]+)(?: \\(aliases: )?([^),]+,?)?+.*$' '$1 $2' |
    string split ' ' -n |
    string trim -c ','
end

# The default subcommand is 'prompt'
set -l is_prompt "not __fish_seen_subcommand_from $subcmds || __fish_seen_subcommand_from prompt"
complete -c llm -n $is_prompt -s s -l system -d "System prompt to use"
complete -c llm -n $is_prompt -s m -l model -d "Model to use" -xa "(__fish_llm_models)"
complete -c llm -n $is_prompt -s a -l attachment -d "Attachment to use" -r -a'-'
complete -c llm -n $is_prompt -l at -d "Attachment type" -r
complete -c llm -n $is_prompt -l attachment-type -d "Attachment type" -r
complete -c llm -n $is_prompt -s n -l no-log -d "Don't log to db"
complete -c llm -n $is_prompt -s l -l log -d "Log prompt/reply to db"
complete -c llm -n $is_prompt -s c -l continue -d "Continue most recent conversation"
complete -c llm -n $is_prompt -l key -d "API key to use"
complete -c llm -n $is_prompt -l save -d "Save prompt as template with name" -r
