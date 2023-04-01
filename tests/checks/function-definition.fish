#RUN: %ghoti %s

function stuff --argument a b c
    # This is a comment
    echo stuff
    # This is another comment
end

functions stuff
#CHECK: # Defined in {{.*}}
#CHECK: function stuff --argument a b c
#CHECK:    # This is a comment
#CHECK:    echo stuff
#CHECK:    # This is another comment
#CHECK: end

function commenting

    # line 2

    # line 4

    echo Bye bye says line 6
end

functions commenting
#CHECK: # Defined in {{.*}}
#CHECK: function commenting
#CHECK:     
#CHECK:     # line 2
#CHECK:     
#CHECK:     # line 4
#CHECK:     
#CHECK:     echo Bye bye says line 6
#CHECK: end
