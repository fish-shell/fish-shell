# NAME
#   spec.view - print test output
#
# SYNOPSIS
#   spec.view <test> <result> [OPTIONS] -- <output>
#   spec.view -r --report <test count> <failed tests>
#
# OPTIONS
#   -r --report
#     Print a passed / failed diagnostics report. Verbose assumed.
#
#   -v --verbose
#     Print test full output.
#
#   -h --header
#
#   -t --tab
#     Use to specify the tab indentation of nested tests inside suites.
#
# AUTHORS
#   Jorge Bucaran <@bucaran>
#/
function spec.view
  # Redefine these colors to customize the report.
  set -l _color_default   @white
  set -l _color_pass      @springgreen
  set -l _color_info      @lgray
  set -l _color_fail      @red
  set -l _color_fail_info @black:yellow
  set -l _color_dump      555

  set -l verbose ""
  if contains -- $argv[1] -r --report
    set -l total_tests  $argv[2]
    set -l failed_tests $argv[3]

    set -l passed_tests (math $total_tests - $failed_tests)

    test $passed_tests -gt 0
      and msg \n $_color_pass "$passed_tests passing"

    test $failed_tests -gt 0
      and msg $_color_fail "$failed_tests failing"
    echo # Blank link

    return 0
  end

  # Use test name or -d description field if available.
  set -l info (type $argv[1] \
    | head -n 2       \
    | tail -n 1       \
    | cut -d" " -f4-  \
    | sed "s/^\'//g"  \
    | sed "s/\'\$//g")

  if [ -z "$info" ]
    set info (echo $argv[1] | tr "_" " ")
  end

  set -l result $argv[2]
  set -l output

  # Parse format options.
  set -l tab      1
  set -l header  ""
  if set -q argv[3]
    for index in (seq 3 (count $argv))
      switch "$argv[$index]"
        case -v --verbose
          set verbose -v
        case -h --header
          set header $info
        case -t --tab
          set tab $argv[(math $index + 1)]
        case --
          if [ $index -lt (count $argv) ]
            set output $argv[(math $index + 1)..-1]
          end
          break
      end
    end
  end

  # Print a header if available.
  if [ -n "$header" -a "$verbose" ]
    msg $_color_default _$header\_
  end

  # Process test functions with output. Typically it_ tests using `expect`.
  if [ -n "$output" ]
    set tab (printf " %.0s" (seq $tab))
    # Process succesful tests.
    if [ $result -eq 0 ]
      if [ -n "$verbose" ]
        msg $tab $_color_pass ✔ $_color_info $info
      else
        echo -n (set_color 00FF7F).
      end
    else
      msg $tab $_color_fail ✖ $_color_fail_info $info

      # Error dump parser, split output by \t.
      set -l index (contains -i -- \t $output)

      # Transform condition string to TitleCase
      set condition (echo $output[(math $index+1)] \
        | tr "-" " " \
        | cut -c 3-  \
        | awk '{for(j=1;j<=NF;j++){ $j=toupper(substr($j,1,1)) substr($j,2) }}1')

      msg $tab __Expected__
      printf (set_color $_color_dump)"$tab  %s\n" $output[1..(math $index-1)]
      msg $tab $condition
      printf (set_color $_color_dump)"$tab  %s\n" $output[(math $index+2)..-1]
    end
  end
end
