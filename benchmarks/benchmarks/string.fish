for i in (seq 100000)
    string match -r '^.*$' fooooooo
end | string match -re o
