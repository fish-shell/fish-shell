function fish_jj_prompt
    # If jj isn't installed, there's nothing we can do
    # Return 1 so the calling prompt can deal with it
    if not command -sq jj
        return 1
    end
    jj log 2>/dev/null --no-graph --ignore-working-copy --color=always --revisions @ \
        --template '
            concat(
                " ",
                separate(" ",
                    format_short_change_id_with_hidden_and_divergent_info(self),
                    bookmarks,
                    tags,
                    if(conflict, label("conflict", "×")),
                    if(empty, label("empty", "(empty)"))
                ),
            )'
end
