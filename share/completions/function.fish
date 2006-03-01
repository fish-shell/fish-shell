complete -c function -s d -l description -d (N_ "Set function description") -x
complete -c function -xa "(functions -n)" -d (N_ "Function")
complete -c function -xa "(builtin -n)" -d (N_ "Builtin")
complete -c function -s j -l on-job-exit -d (N_ "Make the function a job exit event handler") -x
complete -c function -s p -l on-process-exit -d (N_ "Make the function a process exit event handler") -x
complete -c function -s s -l on-signal -d (N_ "Make the function a signal event handler") -x
complete -c function -s v -l on-variable -d (N_ "Make the function a variable update event handler") -x
complete -c function -s b -l key-binding -d (N_ "Allow dash (-) in function name")
