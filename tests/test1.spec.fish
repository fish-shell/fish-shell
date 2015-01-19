function describe_aliases
  function it_supports_aliases
    function foo
        echo >foo.txt $argv
    end

    foo hello

    cat foo.txt |read foo

    expect $foo --to-equal hello
  end
end

function describe_loops
  function it_supports_simple_loops
    set -l output

    for i in \
        a b c
        set output "$output$i"
    end

    expect $output --to-equal 'abc'
  end

  function it_supports_nested_loops
    set -l output

    for i in 1 2 #Comment on same line as command
    #Comment inside loop
        for j in a b
        #Double loop
            set output "$output $i$j"
      end;
    end

    expect $output --to-equal " 1a 1b 2a 2b"
  end
end

#function describe_conditionals
#end

function describe_basic_elements
  function it_supports_simple_bracket_expansion
    expect (echo x-{1}) --to-equal 'x-1'
  end

  function it_supports_double_bracket_expansion
    expect (echo x-{1,2}) --to-equal 'x-1 x-2'
  end

  function it_supports_nested_bracket_expansion
    expect (echo foo-{1,2{3,4}}) --to-equal 'foo-1 foo-23 foo-24'
  end

  function it_supports_whitespace_escaping
    expect (echo foo\ bar) --to-equal 'foo bar'
  end

  function it_supports_newline_escaping
    set -l output (echo foo\
bar)
    expect $output --to-equal 'foobar'
  end

  function it_supports_newline_escaping_surrounded_with_double_quotes
    set -l output (echo "foo\
bar")
    expect $output --to-equal 'foobar'
  end

  function it_does_not_support_newline_escaping_surrounded_with_single_quotes
    set -l output (echo 'foo\
bar')
    expect $output --to-equal (echo -es 'foo\\' '\nbar')
  end
end

spec.run $argv
