function __ssh_history_completions -d "Retrieve `user@host` entries from history"
    history --prefix ssh --max=100 | string replace -rf '.* ([A-Za-z0-9._:-]+@[A-Za-z0-9._:-]+).*' '$1'
end
