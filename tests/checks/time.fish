#RUN: %fish %s
time sleep 1s

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
