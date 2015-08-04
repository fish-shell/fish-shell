function suspend -d "Suspend the current shell so long as it is not a login shell."
    if status --is-login
        echo cannot suspend login shell >&2
    else
        kill -STOP %self
    end
end
