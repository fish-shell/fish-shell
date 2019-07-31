#RUN: %fish -C "set fish %fish" %s
# Check that switch with an argument expanding to nothing still works.
switch $foo
    case a
        echo a
    case ''
        echo very little
    case '*'
        echo Nothin
    case ''
        echo banana
end
#CHECK: very little

switch (true)
    case a
        echo a
    case ''
        echo very little
    case '*'
        echo Nothin
    case ''
        echo banana
end
#CHECK: very little

switch (echo)
    case a
        echo a
    case ''
        echo very little
    case '*'
        echo Nothin
    case ''
        echo banana
end
#CHECK: very little

# Multiple arguments is still an error, we've not decided on good behavior for this.
switch (echo; echo; echo)
    case a
        echo a
    case ""
        echo very little
    case "*"
        echo Nothin
    case ""
        echo banana
end
#CHECKERR: {{.*/?}}switch.fish (line {{\d+}}): switch: Expected at most one argument, got 3
#CHECKERR:
#CHECKERR: switch (echo; echo; echo)
#CHECKERR:       ^

# As is no argument at all.
# Because this is a syntax error, we need to start a sub-fish or we wouldn't execute anything else.
$fish -c '
switch
    case a
        echo a
    case ""
        echo very little
    case "*"
        echo Nothin
    case ""
        echo banana
end
'
#CHECKERR: fish: 'case' builtin not inside of switch block
#CHECKERR:      case a
#CHECKERR:           ^
