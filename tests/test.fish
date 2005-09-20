#!/usr/local/bin/fish
#
# Main loop of the test suite. I wrote this
# instad of using autotest to provide additional 
# testing for fish. :-)

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

