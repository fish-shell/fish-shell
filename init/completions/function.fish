complete -c function -s d -l description -d "Set function description" -x
complete -c function -xa "(functions -n)" -d "Function"
complete -c function -xa "(builtin -n)" -d "Builtin"
complete -c function -s j -l on-job-exit -d "Make the function a job exit event handler" -x
complete -c function -s p -l on-process-exit -d "Make the function a process exit event handler" -x
complete -c function -s s -l on-signal -d "Make the function a signal event handler" -x
complete -c function -s v -l on-variable -d "Make the function a variable update event handler" -x
