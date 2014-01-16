#!/usr/local/bin/fish
#
# Main loop of the test suite. I wrote this
# instad of using autotest to provide additional
# testing for fish. :-)


if [ "$argv" != '-n' ]
  # begin...end has bug in error redirecting...
  begin
    ../fish -n ./test.fish ^top.tmp.err
    ../fish -n ./test.fish -n ^^top.tmp.err
    ../fish ./test.fish -n ^^top.tmp.err
  end | tee top.tmp.out
  echo $status >top.tmp.status
  set res ok
  if diff top.tmp.out top.out >/dev/null
  else
	set res fail
	echo Output differs for file test.fish
  end

  if diff top.tmp.err top.err >/dev/null
  else
	set res fail
	echo Error output differs for file test.fish
  end

  if test (cat top.tmp.status) = (cat top.status)
  else
	set res fail
	echo Exit status differs for file test.fish
  end

  ../fish -p /dev/null -c 'echo testing' >/dev/null
  if test $status -ne 0
	set res fail
	echo Profiling fails
  end

  if test $res = ok;
	echo File test.fish tested ok
        exit 0
  else
	echo File test.fish failed tests
        exit 1
  end;
end

echo Testing high level script functionality

for i in *.in
  set template_out (basename $i .in).out
  set template_err (basename $i .in).err
  set template_status (basename $i .in).status

  ../fish <$i >tmp.out ^tmp.err
  echo $status >tmp.status
  set res ok
  if diff tmp.out $template_out >/dev/null
  else
	set res fail
	echo Output differs for file $i
  end

  if diff tmp.err $template_err >/dev/null
  else
	set res fail
	echo Error output differs for file $i
  end

  if test (cat tmp.status) = (cat $template_status)
  else
	set res fail
	echo Exit status differs for file $i
  end

  if test $res = ok;
	echo File $i tested ok
  else
	echo File $i failed tests
  end;

end

