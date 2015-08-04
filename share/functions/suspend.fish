function suspend
    if status --is-login
        echo cannot suspend login shell >&2
    else
        kill -STOP %self
    end
end
