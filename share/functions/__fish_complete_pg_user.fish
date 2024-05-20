function __fish_complete_pg_user
    psql -AtqwXc 'select usename from pg_user' template1 2>/dev/null
end
