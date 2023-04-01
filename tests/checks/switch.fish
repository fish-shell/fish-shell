#RUN: %ghoti -C "set ghoti %ghoti" %s
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
#CHECKERR: {{.*/?}}switch.ghoti (line {{\d+}}): switch: Expected at most one argument, got 3
#CHECKERR:
#CHECKERR: switch (echo; echo; echo)
#CHECKERR:        ^~~~~~~~~~~~~~~~~^

# As is no argument at all.
# Because this is a syntax error, we need to start a sub-ghoti or we wouldn't execute anything else.
$ghoti -c '
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
#CHECKERR: ghoti: 'case' builtin not inside of switch block
#CHECKERR:      case a
#CHECKERR:      ^~~^

set smurf green

switch $smurf
    case "*ee*"
        echo Test 1 pass
    case "*"
        echo Test 1 fail
end
#CHECK: Test 1 pass

switch $smurf
    case *ee*
        echo Test 2 fail
    case red green blue
        echo Test 2 pass
    case "*"
        echo Test 2 fail
end
#CHECKERR: {{.*}}switch.ghoti (line {{\d+}}): No matches for wildcard '*ee*'. See `help wildcards-globbing`.
#CHECKERR: case *ee*
#CHECKERR:      ^~~^
#CHECK: Test 2 pass

switch $smurf
    case cyan magenta yellow
        echo Test 3 fail
    case "*"
        echo Test 3 pass
end
#CHECK: Test 3 pass

begin
    set -l PATH
    switch (doesnotexist)
        case '*'
            echo Matched!
    end
    # CHECKERR: ghoti: Unknown command: doesnotexist
    # CHECKERR: checks/switch.ghoti (line {{\d+}}):
    # CHECKERR: doesnotexist
    # CHECKERR: ^~~~~~~~~~~^
    # CHECKERR: in command substitution
    # CHECKERR: {{\t}}called on line {{\d+}} of file checks/switch.ghoti
    # CHECKERR: checks/switch.ghoti (line {{\d+}}): Unknown command
    # CHECKERR: switch (doesnotexist)
    # CHECKERR:        ^~~~~~~~~~~~~^
end
