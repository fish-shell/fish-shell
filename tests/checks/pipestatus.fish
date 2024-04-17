#RUN: %fish %s

# pipestatus variable - builtins only
false | false | false
echo $pipestatus : $status
#CHECK: 1 1 1 : 1
true | true | true
echo $pipestatus : $status
#CHECK: 0 0 0 : 0
false | true | false
echo $pipestatus : $status
#CHECK: 1 0 1 : 1
true | false | true
echo $pipestatus : $status
#CHECK: 0 1 0 : 0

# pipestatus variable - no builtins
# Note: On some systems `command false` fails with 255, not 1. We allow both.
command false | command false | command false
echo $pipestatus : $status
#CHECK: {{1|255}} {{1|255}} {{1|255}} : {{1|255}}
command true | command true | command true
echo $pipestatus : $status
#CHECK: 0 0 0 : 0
command false | command true | command false
echo $pipestatus : $status
#CHECK: {{1|255}} 0 {{1|255}} : {{1|255}}
command true | command false | command true
echo $pipestatus : $status
#CHECK: 0 {{1|255}} 0 : 0

# pipestatus variable - mixed
command false | command false | false
echo $pipestatus : $status
#CHECK: {{1|255}} {{1|255}} 1 : 1
command true | true | command true
echo $pipestatus : $status
#CHECK: 0 0 0 : 0
false | command true | command false
echo $pipestatus : $status
#CHECK: 1 0 {{1|255}} : {{1|255}}
true | false | command true
echo $pipestatus : $status
#CHECK: 0 1 0 : 0
sh -c 'exit 5' | sh -c 'exit 2'
echo $pipestatus : $status
#CHECK: 5 2 : 2
sh -c 'exit 3' | false | sh -c 'exit 6'
echo $pipestatus : $status
#CHECK: 3 1 6 : 6
sh -c 'exit 9' | true | sh -c 'exit 3' | false
echo $pipestatus : $status
#CHECK: 9 0 3 1 : 1

# pipestatus variable - non-pipe
true
echo $pipestatus : $status
#CHECK: 0 : 0
false
echo $pipestatus : $status
#CHECK: 1 : 1
command true
echo $pipestatus : $status
#CHECK: 0 : 0
command false
echo $pipestatus : $status
#CHECK: {{1|255}} : {{1|255}}
sh -c 'exit 4'
echo $pipestatus : $status
#CHECK: 4 : 4

# pipestatus variable - negate
! true
echo $pipestatus : $status
#CHECK: 0 : 1
! false
echo $pipestatus : $status
#CHECK: 1 : 0
! false | false | false
echo $pipestatus : $status
#CHECK: 1 1 1 : 0
! true | command true | true
echo $pipestatus : $status
#CHECK: 0 0 0 : 1
! false | true | command false
echo $pipestatus : $status
#CHECK: 1 0 {{1|255}} : 0
! command true | command false | command true
echo $pipestatus : $status
#CHECK: 0 {{1|255}} 0 : 1
! sh -c 'exit 9' | true | sh -c 'exit 3'
echo $pipestatus : $status
#CHECK: 9 0 3 : 0

# pipestatus variable - block
begin
    true
end
echo $pipestatus : $status
#CHECK: 0 : 0
begin
    false
end
echo $pipestatus : $status
#CHECK: 1 : 1
begin
    ! true
end
echo $pipestatus : $status
#CHECK: 0 : 1
begin
    ! false
end
echo $pipestatus : $status
#CHECK: 1 : 0
true | begin
    true
end
echo $pipestatus : $status
#CHECK: 0 0 : 0
false | begin
    false
end
echo $pipestatus : $status
#CHECK: 1 1 : 1
true | begin
    ! true
end
echo $pipestatus : $status
#CHECK: 0 1 : 1
false | begin
    ! false
end
echo $pipestatus : $status
#CHECK: 1 0 : 0
begin
    true | false
end
echo $pipestatus : $status
#CHECK: 0 1 : 1
begin
    false | true
end
echo $pipestatus : $status
#CHECK: 1 0 : 0
begin
    ! true
end | false
echo $pipestatus : $status
#CHECK: 1 1 : 1
begin
    ! false
end | true
echo $pipestatus : $status
#CHECK: 0 0 : 0
begin
    sh -c 'exit 3'
end | begin
    sh -c 'exit 5'
end
echo $pipestatus : $status
#CHECK: 3 5 : 5
begin
    ! sh -c 'exit 3'
end | begin
    sh -c 'exit 5'
end
echo $pipestatus : $status
#CHECK: 0 5 : 5
begin
    ! sh -c 'exit 3'
end | begin
    ! sh -c 'exit 5'
end
echo $pipestatus : $status
#CHECK: 0 0 : 0

# Check that failed redirections correctly handle pipestatus, etc.
# See #7540.
command true > /not/a/valid/path
echo $pipestatus : $status
#CHECK: 1 : 1
#CHECKERR: warning: An error occurred while redirecting file '/not/a/valid/path'
#CHECKERR: warning: Path '/not' does not exist

# Here the first process will launch, the second one will not.
command true | command true | command true > /not/a/valid/path
echo $pipestatus : $status
#CHECK: 0 0 1 : 1
#CHECKERR: warning: An error occurred while redirecting file '/not/a/valid/path'
#CHECKERR: warning: Path '/not' does not exist

# Pipeline breaks do not result in dangling jobs.
command true | command cat > /not/a/valid/path ; jobs
#CHECKERR: warning: An error occurred while redirecting file '/not/a/valid/path'
#CHECKERR: warning: Path '/not' does not exist
#CHECK: jobs: There are no jobs

# Regression test for #7038
cat /dev/zero | dd > /not/a/valid/path
echo 'Not hung'
#CHECKERR: warning: An error occurred while redirecting file '/not/a/valid/path'
#CHECKERR: warning: Path '/not' does not exist
#CHECK: Not hung
