function __fish_complete_pg_database
    psql -AtqwlXF \t 2>/dev/null | awk 'NF > 1 { print $1 }'
end
