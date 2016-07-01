function suspend -d "Suspend the current shell."
    if begin contains -- $argv --force
             or not status --is-interactive and not status --is-login
         end
         echo "Suspending" %self
         echo -n (set_color --bold)" fg"(set_color normal) to resume
         if contains -- $argv --force
                echo " (or"(set_color --bold) "kill -CONT" %self (set_color normal)"from another terminal)"
         end

         kill -STOP %self
    else
         echo 2>&1 "Refusing to suspend login shell. Use --force to override."
         echo 2>&1 "This might hang your terminal."
    end
end
