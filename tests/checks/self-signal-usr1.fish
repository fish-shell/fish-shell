#RUN: %fish %s

# See #6397

function f --on-signal USR1
    begin
        echo a
        echo b
    end | while read -l line
        echo $line
    end
end

kill -USR1 $fish_pid

#CHECK: a
#CHECK: b
