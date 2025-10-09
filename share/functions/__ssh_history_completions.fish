# localization: tier3
function __ssh_history_completions -d "Retrieve `user@host` entries from history"
    # Accept the typical hostname/ip chars, but no ":" at the end
    history --prefix ssh --max=100 | string replace -rf '.* ([A-Za-z0-9._:-]+@[A-Za-z0-9._:-]*[A-Za-z0-9._-]).*' '$1'
end
