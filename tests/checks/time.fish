#RUN: fish=%fish %fish %s
time sleep 0

# These are a tad awkward because it picks the correct unit and adapts whitespace.
# The idea is that it's a table.
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
#CHECKERR: sys time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
time for i in (seq 1 2)
    echo banana
end

#CHECK: banana
#CHECK: banana
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
#CHECKERR: sys time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}

# Make sure we're not double-parsing
time echo 'foo -s   bar'
#CHECK: foo -s   bar
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
#CHECKERR: sys time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}

true && time a=b not builtin true | true
#CHECKERR: ___{{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

not time true
#CHECKERR: ___{{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

not time a=b true
#CHECKERR: ___{{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

# Currently illegal syntax. Same in zsh. POSIX shells call the external command "time" here.
a=b time true
#CHECKERR: fish: time: missing man page
#CHECKERR: Documentation may not be installed.
#CHECKERR: `help time` will show an online version
not a=b time true
#CHECKERR: fish: time: missing man page
#CHECKERR: Documentation may not be installed.
#CHECKERR: `help time` will show an online version

$fish -c 'time true&'
#CHECKERR: fish: {{.*}}
#CHECKERR: time true&
#CHECKERR: ^~~~~~~~~^

$fish -c 'not time true&'
#CHECKERR: fish: {{.*}}
#CHECKERR: not time true&
#FIXME: This error marks the entire statement. Would be cool to mark just `time true&`.
#CHECKERR: ^~~~~~~~~~~~~^

$fish -c 'echo Is it time yet | time cat'
#CHECKERR: fish: The 'time' command may only be at the beginning of a pipeline
#CHECKERR: echo Is it time yet | time cat
#CHECKERR:                       ^~~~~~~^

begin
    printf '%s\n' "#!/bin/sh" 'echo No this is Patrick' > time
    chmod +x time
    set -l PATH .
    echo Hello is this time | command time
    # CHECK: No this is Patrick
end
rm time
