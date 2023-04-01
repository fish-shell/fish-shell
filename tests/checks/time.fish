#RUN: %ghoti -C 'set -l ghoti %ghoti' %s
time sleep 0

# These are a tad awkward because it picks the correct unit and adapts whitespace.
# The idea is that it's a table.
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}ghoti {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
#CHECKERR: sys time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
time for i in (seq 1 2)
    echo banana
end

#CHECK: banana
#CHECK: banana
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}ghoti {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}
#CHECKERR: sys time {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}} {{[\d,.\s]*}} {{millis|micros|secs}}

# Make sure we're not double-parsing
time echo 'foo -s   bar'
#CHECK: foo -s   bar
#CHECKERR: ________________________________________________________
#CHECKERR: Executed in {{[\d,.\s]*}} {{millis|micros|secs}} {{\s*}}ghoti {{\s*}}external
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

$ghoti -c 'time true&'
#CHECKERR: ghoti: {{.*}}
#CHECKERR: time true&
#CHECKERR: ^~~~~~~~~^

$ghoti -c 'not time true&'
#CHECKERR: ghoti: {{.*}}
#CHECKERR: not time true&
#FIXME: This error marks the entire statement. Would be cool to mark just `time true&`.
#CHECKERR: ^~~~~~~~~~~~~^
