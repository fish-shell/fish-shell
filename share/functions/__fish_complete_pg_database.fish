function __fish_complete_pg_database
    psql -AtqwlF \t 2>/dev/null | awk 'NF > 1 { print $1 }'
end
