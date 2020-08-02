#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt, expect_str = sp.sendline, sp.expect_prompt, sp.expect_str

# Test fish_postexec and $status_generation for interactive shells.
expect_prompt()

sendline("function test_fish_postexec --on-event fish_postexec; printf 'pipestatus:%s, generation:%d, command:%s\\n' (string join '|' $pipestatus) $status_generation $argv; end")
expect_prompt()

generation = 1

# fish_postexec is called when foreground job ends.
sendline("true")
generation += 1
expect_str("pipestatus:0, generation:%d, command:true" % generation)
expect_prompt()

# Command has multiple jobs.
sendline("true;false")
generation += 1
expect_str("pipestatus:1, generation:%d, command:true;false" % generation)
expect_prompt()

# Command is a pipeline.
sendline("true|false")
generation += 1
expect_str("pipestatus:0|1, generation:%d, command:true|false" % generation)
expect_prompt()

# Does not increment $status_generation for background jobs.
# status, pipestatus remain unchanged
sendline("sleep 1000 &")
expect_str("pipestatus:0|1, generation:%d, command:sleep 1000 &" % generation)
expect_prompt()

# multiple backgrounded jobs
sendline("sleep 1000 &; sleep 2000 &")
expect_str("pipestatus:0|1, generation:%d, command:sleep 1000 &; sleep 2000 &" % generation)
expect_prompt()

# valid variable assignment
sendline("set foo bar")
expect_str("pipestatus:0|1, generation:%d, command:set foo bar" % generation)
expect_prompt()

# valid variable assignment with background job
sendline("set foo bar; sleep 1000 &")
expect_str("pipestatus:0|1, generation:%d, command:set foo bar; sleep 1000 &" % generation)
expect_prompt()

# Increments $status_generation if any job was foreground.
sendline("false|true; sleep 1000 &")
generation += 1
expect_str("pipestatus:1|0, generation:%d, command:false|true; sleep 1000 &" % generation)
expect_prompt()

sendline("sleep 1000 &; true|false|true")
generation += 1
expect_str("pipestatus:0|1|0, generation:%d, command:sleep 1000 &; true|false|true" % generation)
expect_prompt()

# Increments $status_generation for empty if/while blocks.
sendline("if true;end")
generation += 1
expect_str("pipestatus:0, generation:%d, command:if true;end" % generation)
expect_prompt()

sendline("while false;end")
generation += 1
expect_str("pipestatus:0, generation:%d, command:while false;end" % generation)
expect_prompt()

# or non-matching if.
sendline("if false;false;end")
generation += 1
expect_str("pipestatus:0, generation:%d, command:if false;false;end" % generation)
expect_prompt()

# or a function definition.
# This is an implementation detail, but the test case should prevent regressions.
sendline("function fail; false; end")
generation += 1
expect_str("pipestatus:0, generation:%d, command:function fail; false; end" % generation)
expect_prompt()

# or an invalid variable assignment
sendline("set '!@#$' value")
generation += 1
expect_str("pipestatus:2, generation:%d, command:set '!@#$' value" % generation)
expect_prompt()

# or a variable query
sendline("set -q fish_pid")
generation += 1
expect_str("pipestatus:0, generation:%d, command:set -q fish_pid" % generation)
expect_prompt()

# This is just to set a memorable pipestatus.
sendline("true|false|true")
generation += 1
expect_str("pipestatus:0|1|0")
expect_prompt()

# Does not increment $status_generation for empty begin/end block.
sendline("begin;end")
expect_str("pipestatus:0|1|0, generation:%d, command:begin;end" % generation)
expect_prompt()

# Or begin/end block with only backgrounded jobs.
sendline("begin; sleep 200 &; sleep 400 &; end")
expect_str("pipestatus:0|1|0, generation:%d, command:begin; sleep 200 &; sleep 400 &; end" % generation)
expect_prompt()

# Or a combination of begin/end block and backgrounded job.
sendline("begin; sleep 200 &; end; sleep 400 &")
expect_str("pipestatus:0|1|0, generation:%d, command:begin; sleep 200 &; end; sleep 400 &" % generation)
expect_prompt()

# Or a combination with variable assignments
sendline("begin; set foo bar; sleep 1000 &; end; set bar baz; sleep 2000 &")
expect_str("pipestatus:0|1|0, generation:%d, command:begin; set foo bar; sleep 1000 &; end; set bar baz; sleep 2000 &" % generation)
expect_prompt()
