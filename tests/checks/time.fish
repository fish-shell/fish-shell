#RUN: %fish -C 'set -l fish %fish' %s
time sleep 0

# These are a tad awkward because it picks the correct unit and adapts whitespace.
# The idea is that it's a table.
#CHECKERR: ________________________________________________________
#CHECKERR: Duration {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: user time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: kernel time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
time for i in (seq 1 2)
    echo banana
end

#CHECK: banana
#CHECK: banana
#CHECKERR: ________________________________________________________
#CHECKERR: Duration {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: user time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: kernel time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}

# Make sure we're not double-parsing
time echo 'foo -s   bar'
#CHECK: foo -s   bar
#CHECKERR: ________________________________________________________
#CHECKERR: Duration {{[\d,.\s]*}} {{ms|μs|sec}} {{\s*}}fish {{\s*}}external
#CHECKERR: user time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}
#CHECKERR: kernel time {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}} {{[\d,.\s]*}} {{ms|μs|sec}}

true && time a=b not builtin true | true
#CHECKERR: ___{{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}
#CHECKERR: {{.*}}

PATH= time true
#CHECKERR: fish: Unknown command: time
#CHECKERR: {{.*}}
#CHECKERR: PATH= time true
#CHECKERR:       ^

not time true
#CHECKERR: ___{{.*}}
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
