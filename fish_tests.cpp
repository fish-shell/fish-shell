/** \file fish_tests.c
	Various bug and feature tests. Compiled and run by make test.
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <algorithm>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <signal.h>

#include <locale.h>
#include <dirent.h>
#include <time.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "proc.h"
#include "reader.h"
#include "builtin.h"
#include "function.h"
#include "autoload.h"
#include "complete.h"
#include "wutil.h"
#include "env.h"
#include "expand.h"
#include "parser.h"
#include "tokenizer.h"
#include "output.h"
#include "exec.h"
#include "event.h"
#include "path.h"
#include "history.h"
#include "highlight.h"
#include "iothread.h"
#include "postfork.h"
#include "signal.h"
/**
   The number of tests to run
 */
//#define ESCAPE_TEST_COUNT 1000000
#define ESCAPE_TEST_COUNT 10000
/**
   The average length of strings to unescape
 */
#define ESCAPE_TEST_LENGTH 100
/**
   The higest character number of character to try and escape
 */
#define ESCAPE_TEST_CHAR 4000

/**
   Number of laps to run performance testing loop
*/
#define LAPS 50

/**
   The result of one of the test passes
*/
#define NUM_ANS L"-7 99999999 1234567 deadbeef DEADBEEFDEADBEEF"

/**
   Number of encountered errors
*/
static int err_count=0;

/**
   Print formatted output
*/
static void say( const wchar_t *blah, ... )
{
	va_list va;
	va_start( va, blah );
	vwprintf( blah, va );
	va_end( va );	
	wprintf( L"\n" );
}

/**
   Print formatted error string
*/
static void err( const wchar_t *blah, ... )
{
	va_list va;
	va_start( va, blah );
	err_count++;
	
	wprintf( L"Error: " );
	vwprintf( blah, va );
	va_end( va );	
	wprintf( L"\n" );
}

/**
   Test the escaping/unescaping code by escaping/unescaping random
   strings and verifying that the original string comes back.
*/
static void test_escape()
{
	int i;
	wcstring sb;
	
	say( L"Testing escaping and unescaping" );
	
	for( i=0; i<ESCAPE_TEST_COUNT; i++ )
	{
		const wchar_t *o, *e, *u;
		
		sb.clear();
		while( rand() % ESCAPE_TEST_LENGTH )
		{
			sb.push_back((rand() %ESCAPE_TEST_CHAR) +1 );
		}
		o = (const wchar_t *)sb.c_str();
		e = escape(o, 1);
		u = unescape( e, 0 );
		if( !o || !e || !u )
		{
			err( L"Escaping cycle of string %ls produced null pointer on %ls", o, e?L"unescaping":L"escaping" );
			
		}
		
			
		if( wcscmp(o, u) )
		{
			err( L"Escaping cycle of string %ls produced different string %ls", o, u );
			
			
		}
		free( (void *)e );
		free( (void *)u );
		
	}		
}

static void test_format(void) {
    say( L"Testing formatting functions" );
    struct { unsigned long long val; const char *expected; } tests[] = {
        { 0, "empty" },
        { 1, "1B" },
        { 2, "2B" },
        { 1024, "1kB" },
        { 1870, "1.8kB" },
        { 4322911, "4.1MB" }
    };
    size_t i;
    for (i=0; i < sizeof tests / sizeof *tests; i++) {
        char buff[128];
        format_size_safe(buff, tests[i].val);
        assert( ! strcmp(buff, tests[i].expected));
    }
    
    for (int j=-129; j <= 129; j++) {
        char buff1[128], buff2[128];
        format_int_safe(buff1, j);
        sprintf(buff2, "%d", j);
        assert( ! strcmp(buff1, buff2));
    }    
}

/**
   Test wide/narrow conversion by creating random strings and
   verifying that the original string comes back thorugh double
   conversion.
*/
static void test_convert()
{
/*	char o[] = 
		{
			-17, -128, -121, -68, 0
		}
	;
	
	wchar_t *w = str2wcs(o);
	char *n = wcs2str(w);

	int i;
	
	for( i=0; o[i]; i++ )
	{
		bitprint(o[i]);;
		//wprintf(L"%d ", o[i]);
	}
	wprintf(L"\n");

	for( i=0; w[i]; i++ )
	{
		wbitprint(w[i]);;
		//wprintf(L"%d ", w[i]);
	}
	wprintf(L"\n");

	for( i=0; n[i]; i++ )
	{
		bitprint(n[i]);;
		//wprintf(L"%d ", n[i]);
	}
	wprintf(L"\n");

	return;
*/


	int i;
	buffer_t sb;
	
	say( L"Testing wide/narrow string conversion" );

	b_init( &sb );
	
	for( i=0; i<ESCAPE_TEST_COUNT; i++ )
	{
		wchar_t *w;
		char *o, *n;
		
		char c;
		
		sb.used=0;
		
		while( rand() % ESCAPE_TEST_LENGTH )
		{
			c = rand ();
			b_append( &sb, &c, 1 );
		}
		c = 0;
		b_append( &sb, &c, 1 );
		
		o = (char *)sb.buff;
		w = str2wcs(o);
		n = wcs2str(w);
		
		if( !o || !w || !n )
		{
			err( L"Line %d - Conversion cycle of string %s produced null pointer on %s", __LINE__, o, w?L"str2wcs":L"wcs2str" );
		}
		
		if( strcmp(o, n) )
		{
			err( L"Line %d - %d: Conversion cycle of string %s produced different string %s", __LINE__, i, o, n );
		}
		free( w );
		free( n );
		
	}

}

/**
   Test the tokenizer
*/
static void test_tok()
{
	tokenizer t;
	
	say( L"Testing tokenizer" );

	
	say( L"Testing invalid input" );
	tok_init( &t, 0, 0 );

	if( tok_last_type( &t ) != TOK_ERROR )
	{
		err(L"Invalid input to tokenizer was undetected" );
	}
	
	say( L"Testing use of broken tokenizer" );
	if( !tok_has_next( &t ) )
	{
		err( L"tok_has_next() should return 1 once on broken tokenizer" );
	}
	
	tok_next( &t );
	if( tok_last_type( &t ) != TOK_ERROR )
	{
		err(L"Invalid input to tokenizer was undetected" );
	}

	/*
	  This should crash if there is a bug. No reliable way to detect otherwise.
	*/
	say( L"Test destruction of broken tokenizer" );
	tok_destroy( &t );
	
	{

		const wchar_t *str = L"string <redirection  2>&1 'nested \"quoted\" '(string containing subshells ){and,brackets}$as[$well (as variable arrays)]";
		const int types[] = 
		{
			TOK_STRING, TOK_REDIRECT_IN, TOK_STRING, TOK_REDIRECT_FD, TOK_STRING, TOK_STRING, TOK_END
		}
		;
		size_t i;
		
		say( L"Test correct tokenization" );

		for( i=0, tok_init( &t, str, 0 ); i<(sizeof(types)/sizeof(int)); i++,tok_next( &t ) )
		{
			if( types[i] != tok_last_type( &t ) )
			{
				err( L"Tokenization error:");
				wprintf( L"Token number %d of string \n'%ls'\n, expected token type %ls, got token '%ls' of type %ls\n", 
						 i+1,
						 str,
						 tok_get_desc(types[i]),
						 tok_last(&t),
						 tok_get_desc(tok_last_type( &t )) );
			}
		}
	}
}

static int test_fork_helper(void *unused) {
    size_t i;
    for (i=0; i < 100000; i++) {
        delete [] (new char[4 * 1024 * 1024]);
    }
    return 0;
}

static void test_fork(void) {
    return; // Test is disabled until I can force it to fail
    say(L"Testing fork");
    size_t i, max = 100;
    for (i=0; i < 100; i++) {
        printf("%lu / %lu\n", i+1, max);
        /* Do something horrible to try to trigger an error */
#define THREAD_COUNT 8
#define FORK_COUNT 200
#define FORK_LOOP_COUNT 16
        signal_block();
        for (size_t i=0; i < THREAD_COUNT; i++) {
            iothread_perform<void>(test_fork_helper, NULL, NULL);
        }
        for (size_t q = 0; q < FORK_LOOP_COUNT; q++) {
            size_t pids[FORK_COUNT];
            for (size_t i=0; i < FORK_COUNT; i++) {
                pid_t pid = execute_fork(false);
                if (pid > 0) {
                    /* Parent */
                    pids[i] = pid;
                } else if (pid == 0) {
                    /* Child */
                    new char[4 * 1024 * 1024];
                    exit_without_destructors(0);
                } else {
                    perror("fork");
                }        
            }
            for (size_t i=0; i < FORK_COUNT; i++) {
                int status = 0;
                if (pids[i] != waitpid(pids[i], &status, 0)) {
                    perror("waitpid");
                    assert(0);
                }
                assert(WIFEXITED(status) && 0 == WEXITSTATUS(status));
            }
        }
        iothread_drain_all();
        signal_unblock();
    }
#undef FORK_COUNT
}

/**
   Test the parser
*/
static void test_parser()
{
	say( L"Testing parser" );
	
	parser_t parser(PARSER_TYPE_GENERAL);
    
	say( L"Testing null input to parser" );
	if( !parser.test( 0, 0, 0, 0 ) )
	{
		err( L"Null input to parser.test undetected" );
	}

	say( L"Testing block nesting" );
	if( !parser.test( L"if; end", 0, 0, 0 ) )
	{
		err( L"Incomplete if statement undetected" );			
	}
	if( !parser.test( L"if test; echo", 0, 0, 0 ) )
	{
		err( L"Missing end undetected" );			
	}
	if( !parser.test( L"if test; end; end", 0, 0, 0 ) )
	{
		err( L"Unbalanced end undetected" );			
	}

	say( L"Testing detection of invalid use of builtin commands" );
	if( !parser.test( L"case foo", 0, 0, 0 ) )
	{
		err( L"'case' command outside of block context undetected" );		
	}
	if( !parser.test( L"switch ggg; if true; case foo;end;end", 0, 0, 0 ) )
	{
		err( L"'case' command outside of switch block context undetected" );		
	}
	if( !parser.test( L"else", 0, 0, 0 ) )
	{
		err( L"'else' command outside of conditional block context undetected" );		
	}
	if( !parser.test( L"break", 0, 0, 0 ) )
	{
		err( L"'break' command outside of loop block context undetected" );		
	}
	if( !parser.test( L"exec ls|less", 0, 0, 0 ) || !parser.test( L"echo|return", 0, 0, 0 ))
	{
		err( L"Invalid pipe command undetected" );		
	}	

	say( L"Testing basic evaluation" );
#if 0
    /* This fails now since the parser takes a wcstring&, and NULL converts to wchar_t * converts to wcstring which crashes (thanks C++) */
	if( !parser.eval( 0, 0, TOP ) )
	{
		err( L"Null input when evaluating undetected" );
	}
#endif
	if( !parser.eval( L"ls", 0, WHILE ) )
	{
		err( L"Invalid block mode when evaluating undetected" );
	}	
}

class lru_node_test_t : public lru_node_t {
    public:
    lru_node_test_t(const wcstring &tmp) : lru_node_t(tmp) { }
};

class test_lru_t : public lru_cache_t<lru_node_test_t> {
    public:
    test_lru_t() : lru_cache_t<lru_node_test_t>(16) { }
    
    std::vector<lru_node_test_t *> evicted_nodes;
    
    virtual void node_was_evicted(lru_node_test_t *node) {
        assert(find(evicted_nodes.begin(), evicted_nodes.end(), node) == evicted_nodes.end());
        evicted_nodes.push_back(node);
    }
};

static void test_lru(void) {
    say( L"Testing LRU cache" );
    
    test_lru_t cache;
    std::vector<lru_node_test_t *> expected_evicted;
    size_t total_nodes = 20;
    for (size_t i=0; i < total_nodes; i++) {
        assert(cache.size() == std::min(i, (size_t)16));
        lru_node_test_t *node = new lru_node_test_t(format_val(i));
        if (i < 4) expected_evicted.push_back(node);
        // Adding the node the first time should work, and subsequent times should fail
        assert(cache.add_node(node));
        assert(! cache.add_node(node));
    }
    assert(cache.evicted_nodes == expected_evicted);
    cache.evict_all_nodes();
    assert(cache.evicted_nodes.size() == total_nodes);
    while (! cache.evicted_nodes.empty()) {
        lru_node_t *node = cache.evicted_nodes.back();
        cache.evicted_nodes.pop_back();
        delete node;
    }
}
	
/**
   Perform parameter expansion and test if the output equals the zero-terminated parameter list supplied.

   \param in the string to expand
   \param flags the flags to send to expand_string
*/

static int expand_test( const wchar_t *in, int flags, ... )
{
	std::vector<completion_t> output;
	va_list va;
	size_t i=0;
	int res=1;
	wchar_t *arg;
	
	if( expand_string( in, output, flags) )
	{
		
	}
	
	
	va_start( va, flags );

	while( (arg=va_arg(va, wchar_t *) )!= 0 ) 
	{
		if( output.size() == i )
		{
			res=0;
			break;
		}
		
        if (output.at(i).completion != arg)
		{
			res=0;
			break;
		}
		
		i++;		
	}
	va_end( va );
	
	return res;
		
}

/**
   Test globbing and other parameter expansion
*/
static void test_expand()
{
	say( L"Testing parameter expansion" );
	
	if( !expand_test( L"foo", 0, L"foo", 0 ))
	{
		err( L"Strings do not expand to themselves" );
	}

	if( !expand_test( L"a{b,c,d}e", 0, L"abe", L"ace", L"ade", 0 ) )
	{
		err( L"Bracket expansion is broken" );
	}

	if( !expand_test( L"a*", EXPAND_SKIP_WILDCARDS, L"a*", 0 ) )
	{
		err( L"Cannot skip wildcard expansion" );
	}
	
}

/**
   Test path functions
 */
static void test_path()
{
	say( L"Testing path functions" );

    wcstring path = L"//foo//////bar/";
    wcstring canon = path;
    path_make_canonical(canon);
	if( canon != L"/foo/bar" )
	{
		err( L"Bug in canonical PATH code" );
	}	
}

/** Testing colors */
static void test_colors()
{
    say(L"Testing colors");
    assert(rgb_color_t(L"#FF00A0").is_rgb());
    assert(rgb_color_t(L"FF00A0").is_rgb());
    assert(rgb_color_t(L"#F30").is_rgb());
    assert(rgb_color_t(L"F30").is_rgb());
    assert(rgb_color_t(L"f30").is_rgb());
    assert(rgb_color_t(L"#FF30a5").is_rgb());
    assert(rgb_color_t(L"3f30").is_none());
    assert(rgb_color_t(L"##f30").is_none());
    assert(rgb_color_t(L"magenta").is_named());
    assert(rgb_color_t(L"MaGeNTa").is_named());
    assert(rgb_color_t(L"mooganta").is_none());
}

/* Testing autosuggestion */
static void test_autosuggest() {
    bool autosuggest_handle_special(const wcstring &str, const wcstring &working_directory, bool *outSuggestionOK);
}


/**
   Test speed of completion calculations
*/
void perf_complete()
{
	wchar_t c;
	std::vector<completion_t> out;
	long long t1, t2;
	int matches=0;
	double t;
	wchar_t str[3]=
	{
		0, 0, 0 
	}
	;
	int i;
	
	
	say( L"Testing completion performance" );
	
	reader_push(L"");
	say( L"Here we go" );
	 	
	t1 = get_time();
	
	
	for( c=L'a'; c<=L'z'; c++ )
	{
		str[0]=c;
		reader_set_buffer( str, 0 );
		
		complete( str, out, COMPLETE_DEFAULT, NULL );
	
		matches += out.size();
        out.clear();
	}
	t2=get_time();
	
	t = (double)(t2-t1)/(1000000*26);
	
	say( L"One letter command completion took %f seconds per completion, %f microseconds/match", t, (double)(t2-t1)/matches );
	
	matches=0;
	t1 = get_time();
	for( i=0; i<LAPS; i++ )
	{
		str[0]='a'+(rand()%26);
		str[1]='a'+(rand()%26);
		
		reader_set_buffer( str, 0 );
		
		complete( str, out, COMPLETE_DEFAULT, NULL );
	
		matches += out.size();
        out.clear();
	}
	t2=get_time();
	
	t = (double)(t2-t1)/(1000000*LAPS);
	
	say( L"Two letter command completion took %f seconds per completion, %f microseconds/match", t, (double)(t2-t1)/matches );
	
	reader_pop();
	
}

static void test_history_matches(history_search_t &search, size_t matches) {
    size_t i;
    for (i=0; i < matches; i++) {
        assert(search.go_backwards());
        wcstring item = search.current_string();
    }
    assert(! search.go_backwards());
    
    for (i=1; i < matches; i++) {
        assert(search.go_forwards());
    }
    assert(! search.go_forwards());
}

class history_tests_t {
public:
    static void test_history(void);
};

static wcstring random_string(void) {
    wcstring result;
    size_t max = 1 + rand() % 32;
    while (max--) {
        wchar_t c = 1 + rand()%ESCAPE_TEST_CHAR;
        result.push_back(c);
    }
    return result;
}

void history_tests_t::test_history(void) {
    say( L"Testing history");
    
    history_t &history = history_t::history_with_name(L"test_history");
    history.clear();
    history.add(L"Gamma");
    history.add(L"Beta");
    history.add(L"Alpha");
    
    /* All three items match "a" */
    history_search_t search1(history, L"a");
    test_history_matches(search1, 3);
    assert(search1.current_string() == L"Alpha");
    
    /* One item matches "et" */
    history_search_t search2(history, L"et");
    test_history_matches(search2, 1);
    assert(search2.current_string() == L"Beta");
    
    /* Test history escaping and unescaping, yaml, etc. */
    std::vector<history_item_t> before, after;
    history.clear();
    size_t i, max = 100;
    for (i=1; i <= max; i++) {
        
        /* Generate a value */
        wcstring value = wcstring(L"test item ") + format_val(i);
        
        /* Maybe add some backslashes */
        if (i % 3 == 0)
            value.append(L"(slashies \\\\\\ slashies)");

        /* Generate some paths */
        path_list_t paths;        
        size_t count = rand() % 6;
        while (count--) {
            paths.push_front(random_string());
        }
        
        /* Record this item */
        history_item_t item(value, time(NULL), paths);
        before.push_back(item);
        history.add(item);
    }
    history.save();
    
    /* Read items back in reverse order and ensure they're the same */
    for (i=100; i >= 1; i--) {
        history_item_t item = history.item_at_index(i);
        assert(! item.empty());
        after.push_back(item);
    }
    assert(before.size() == after.size());
    for (size_t i=0; i < before.size(); i++) {
        const history_item_t &bef = before.at(i), &aft = after.at(i);
        assert(bef.contents == aft.contents);
        assert(bef.creation_timestamp == aft.creation_timestamp);
        assert(bef.required_paths == aft.required_paths);
    }
}


/**
   Main test 
*/
int main( int argc, char **argv )
{
	setlocale( LC_ALL, "" );
	srand( time( 0 ) );

	program_name=L"(ignore)";
	
	say( L"Testing low-level functionality");
	say( L"Lines beginning with '(ignore):' are not errors, they are warning messages\ngenerated by the fish parser library when given broken input, and can be\nignored. All actual errors begin with 'Error:'." );
	set_main_thread();
	proc_init();	
	event_init();	
	function_init();
	builtin_init();
	reader_init();
	env_init();
    
    test_format();
	test_escape();
	test_convert();
	test_tok();
    test_fork();
	test_parser();
	test_lru();
	test_expand();
	test_path();
    test_colors();
    test_autosuggest();
    history_tests_t::test_history();
	
	say( L"Encountered %d errors in low-level tests", err_count );

	/*
	  Skip performance tests for now, since they seem to hang when running from inside make (?)
	*/
//	say( L"Testing performance" );
//	perf_complete();
		
	env_destroy();
	reader_destroy();	
	builtin_destroy();
	wutil_destroy();
	event_destroy();
	proc_destroy();
	
}
