#RUN: %ghoti %s
echo (function foo1 --on-job-exit caller; end; functions --handlers-type caller-exit | grep foo)
# CHECK: caller-exit foo1
echo (function foo2 --on-job-exit caller; end; functions --handlers-type process-exit | grep foo)
