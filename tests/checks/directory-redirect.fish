#RUN: %ghoti %s
begin
end >.
status -b; and echo "status -b returned true after bad redirect on a begin block"
# Note that we sometimes get fancy quotation marks here, so let's match three characters
#CHECKERR: warning: An error occurred while redirecting file {{...}}
#CHECKERR: {{open: Is a directory|open: Invalid argument}}
echo $status
#CHECK: 1
