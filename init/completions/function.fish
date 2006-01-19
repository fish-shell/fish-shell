complete -c function -s d -l description -d (_ "Set function description") -x
complete -c function -xa "(functions -n)" -d (_ "Function")
complete -c function -xa "(builtin -n)" -d (_ "Builtin")
complete -c function -s j -l on-job-exit -d (_ "Make the function a job exit event handler") -x
complete -c function -s p -l on-process-exit -d (_ "Make the function a process exit event handler") -x
complete -c function -s s -l on-signal -d (_ "Make the function a signal event handler") -x
complete -c function -s v -l on-variable -d (_ "Make the function a variable update event handler") -x
complete -c function -s b -l key-binding -d (_ "Allow dash (-) in function name")
