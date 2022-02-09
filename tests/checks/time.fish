#RUN: %fish -C 'set -l fish %fish' %s
time sleep 0

# These are a tad awkward because it picks the correct unit and adapts whitespace.
# The idea is that it's a table.
#CHECKERR: {{.*}}
#CHECKERR: Executed in {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: sys time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
time for i in (seq 1 2)
    echo banana
end

#CHECK: banana
#CHECK: banana
#CHECKERR: {{.*}}
#CHECKERR: Executed in {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: sys time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}

# Make sure we're not double-parsing
time echo 'foo -s   bar'
#CHECK: foo -s   bar
#CHECKERR: {{.*}}
#CHECKERR: Executed in {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: usr time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: sys time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}

true && time a=b not builtin true | true
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

not time true
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

$fish -c 'time true&'
#CHECKERR: fish: {{.*}}
#CHECKERR: time true&
#CHECKERR: ^

$fish -c 'not time true&'
#CHECKERR: fish: {{.*}}
#CHECKERR: not time true&
#CHECKERR: ^
