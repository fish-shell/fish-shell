#!/usr/bin/env python

import sys
import commands
import re

# Regexes for performing cleanup

cl = { re.compile(r"-[ \t]*\n[ \t\r]+" ):"",
       re.compile(r"[ \n\t\r]+", re.MULTILINE):" ",
       re.compile(r"^[ \n\t\r]"):"",
       re.compile(r"[ \n\t\r]$"):"" }

def header(cmd):
  print '''#
# Command specific completions for the %s command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#
''' % (cmd)

def up_first(s):
  return s[0].upper() + s[1:]

def escape_quotes(s):
  return re.sub('\'', '\\\'', s)

def escape(s):
  return re.sub('([\'"#%*?])', r"\\\1", s)

def clean(s):
  res=s
  for r, str in cl.items():
    res = r.sub(str, res)
  return res

def print_completion( cmd, switch_arr, arg, desc ):

  if len(switch_arr)==0:
    return
  
  res = "complete -c %s" % (cmd)
  for sw in switch_arr:
    
    offset=1
    switch_type = "o"
    
    if len(sw) == 2:
      switch_type = "s"

    if sw[1] == "-":
      switch_type = "l"
      offset=2

    res += " -%s %s" % (switch_type, escape(sw[offset:]))

  res += " --description '%s'" % (up_first(escape_quotes(clean(desc))))

  print res

cmd = sys.argv[1]

header(cmd)

man = commands.getoutput( "man %s | col -b" % cmd )

remainder = man

MODE_NONE = 0
MODE_SWITCH = 1
MODE_BETWEEN = 2
MODE_BETWEEN_IGNORE = 3
MODE_DESC = 4

mode = MODE_NONE
pos = 0
sw=''
sw_arr=[]
switch_end="= \t\n[,"
switch_between_ignore="[="
switch_between_continue=" \t\n|"
before_switch=" \t\r"
between_ignore=" \t\n]"
pc=False
desc=''

can_be_switch =True

for c in man:

  if mode == MODE_NONE:
    if c == '-' and can_be_switch:
      mode = MODE_SWITCH
      sw = '-'

    elif c == '\n':
      can_be_switch = True
    elif before_switch.find(c)<0:
      can_be_switch = False
      

  elif mode == MODE_SWITCH:
    if not switch_end.find(c)>=0:
      sw+=c
    else:
      if len(sw) > 1:
        sw_arr.append(sw)
      
      if switch_between_ignore.find(c) >= 0:
        mode=MODE_BETWEEN_IGNORE
      else:
        mode=MODE_BETWEEN
    #  print "End of switch argumnt", sw, "switch to between mode"
      sw=''

  elif mode == MODE_BETWEEN:
    if c == '-':
      mode = MODE_SWITCH
      sw = '-'
    elif switch_between_ignore.find(c) >= 0:
      mode = MODE_BETWEEN_IGNORE
   #   print "Found character", c, "switching to ignore mode"
    elif not switch_between_continue.find(c) >= 0:
      mode = MODE_DESC
      desc = c
        
  elif mode == MODE_BETWEEN_IGNORE:
    if between_ignore.find(c)>=0:
      mode = MODE_BETWEEN

  elif mode == MODE_DESC:

    stop = False

    if c == '.':
      stop = True

    if c == '\n' and pc == '\n':
      stop=True

    if stop:      
      mode=MODE_NONE
      
      print_completion( cmd, sw_arr, None, desc )

      sw_arr = []

      desc = ''
      
    else:
      desc += c

  else:
    print "Unknown mode", mode
      
  pc = c

