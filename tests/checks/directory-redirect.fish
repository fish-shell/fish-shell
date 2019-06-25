#RUN: %fish -c 'begin; end > . ; status -b; and echo "status -b returned true after bad redirect on a begin block"; echo $status'
# Note that we sometimes get fancy quotation marks here, so let's match three characters
#CHECKERR: <W> fish: An error occurred while redirecting file {{...}}
#CHECKERR: {{open: Is a directory|open: Invalid argument}}
#CHECK: 1
