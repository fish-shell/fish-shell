/** \file intern.c

    Library for pooling common strings

*/
#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <set>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "intern.h"

/** Comparison function for intern'd strings */
static bool string_table_compare(const wchar_t *a, const wchar_t *b) {
    return wcscmp(a, b) < 0;
}

/** The table of intern'd strings */
typedef std::set<const wchar_t *, bool (*)(const wchar_t *, const wchar_t *b)> string_table_t;
static string_table_t string_table(string_table_compare);

/** The lock to provide thread safety for intern'd strings */
static pthread_mutex_t intern_lock = PTHREAD_MUTEX_INITIALIZER;

static const wchar_t *intern_with_dup( const wchar_t *in, bool dup )
{
	if( !in )
		return NULL;
        
//	debug( 0, L"intern %ls", in );
    scoped_lock lock(intern_lock);
    const wchar_t *result;
    string_table_t::const_iterator iter = string_table.find(in);
    if (iter != string_table.end()) {
        result = *iter; 
    } else {
        result = dup ? wcsdup(in) : in;
        string_table.insert(result);
    }
    return result;
}

const wchar_t *intern( const wchar_t *in )
{
	return intern_with_dup(in, true);
}


const wchar_t *intern_static( const wchar_t *in )
{
	return intern_with_dup(in, false);
}
