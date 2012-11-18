#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <errno.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "fallback.h"
#include "print_help.h"
#include "color.h"

/*
  Small utility for setting the color.
  Usage: set_color COLOR
  where COLOR is either an integer from 0 to seven or one of the strings in the col array.
*/

#define COLORS (sizeof(col)/sizeof(char *))

/**
   Program name
*/
#define SET_COLOR "set_color"

/**
   Getopt short switches for set_color
*/
#define GETOPT_STRING "b:hvocu"

#ifdef _
 #undef _
#endif

#ifdef USE_GETTEXT
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

const char *col[]=
{
  "black",
  "red",
  "green",
  "brown",
  "yellow",
  "blue",
  "magenta",
  "purple",
  "cyan",
  "white",
  "normal"
};

const int col_idx[]=
{
  0,
  1,
  2,
  3,
  3,
  4,
  5,
  5,
  6,
  7,
  8
};

void print_colors()
{
  size_t i;
  for( i=0; i<COLORS; i++ )
  {
    printf( "%s\n", col[i] );
  }
}

static void check_locale_init()
{
  static int is_init = 0;
  if( is_init )
    return;

  is_init = 1;
  setlocale( LC_ALL, "" );
  bindtextdomain( PACKAGE_NAME, LOCALEDIR );
  textdomain( PACKAGE_NAME );
}

/* A lot of this code is taken straight from output.cpp; it sure would be nice to factor these together. */

static bool support_term256;
static bool output_get_supports_term256(void) {
    return support_term256;
}

static bool term256_support_is_native(void) {
    /* Return YES if we think the term256 support is "native" as opposed to forced. */
    return max_colors == 256;
}

static bool write_color(char *todo, unsigned char idx, bool is_fg) {
    bool result = false;
    if (idx < 16 || term256_support_is_native()) {
        /* Use tparm */
        putp( tparm( todo, idx ) );
        result = true;
    } else {
        /* We are attempting to bypass the term here. Generate the ANSI escape sequence ourself. */
        char stridx[128];
        format_long_safe(stridx, (long)idx);
        char buff[128] = "\x1b[";
        strcat(buff, is_fg ? "38;5;" : "48;5;");
        strcat(buff, stridx);
        strcat(buff, "m");
        write_loop(STDOUT_FILENO, buff, strlen(buff));
        result = true;
    }
    return result;
}

static bool write_foreground_color(unsigned char idx) {
    if (set_a_foreground && set_a_foreground[0]) {
        return write_color(set_a_foreground, idx, true);
    } else if (set_foreground && set_foreground[0]) {
        return write_color(set_foreground, idx, true);
    } else {
        return false;
    }
}

static bool write_background_color(unsigned char idx) {
    if (set_a_background && set_a_background[0]) {
        return write_color(set_a_background, idx, false);
    } else if (set_background && set_background[0]) {
        return write_color(set_background, idx, false);
    } else {
        return false;
    }
}

static unsigned char index_for_color(rgb_color_t c) {
    if (c.is_named() || ! output_get_supports_term256()) {
        return c.to_name_index();
    } else {
        return c.to_term256_index();
    }
}


int main( int argc, char **argv )
{
    /* Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a hack, quietly return failure. */
    if (argc <= 1)
        return EXIT_FAILURE;

  char *bgcolor=0;
  char *fgcolor=0;
  bool bold=false;
  bool underline=false;

  while( 1 )
  {
    static struct option
      long_options[] =
      {
        {
          "background", required_argument, 0, 'b'
        }
        ,
        {
          "help", no_argument, 0, 'h'
        }
        ,
        {
          "bold", no_argument, 0, 'o'
        }
        ,
        {
          "underline", no_argument, 0, 'u'
        }
        ,
        {
          "version", no_argument, 0, 'v'
        }
        ,
        {
          "print-colors", no_argument, 0, 'c'
        }
        ,
        {
          0, 0, 0, 0
        }
      }
    ;

    int opt_index = 0;

    int opt = getopt_long( argc,
                 argv,
                 GETOPT_STRING,
                 long_options,
                 &opt_index );

    if( opt == -1 )
      break;

    switch( opt )
    {
      case 0:
        break;

      case 'b':
        bgcolor = optarg;
        break;
      case 'h':
        print_help( argv[0], 1 );
        exit(0);

      case 'o':
        bold=true;
        break;

      case 'u':
        underline=true;
        break;

      case 'v':
        check_locale_init();
        fprintf( stderr, _("%s, version %s\n"), SET_COLOR, PACKAGE_VERSION );
        exit( 0 );

      case 'c':
        print_colors();
        exit(0);

      case '?':
        return 1;

    }

  }

  switch( argc-optind)
  {
    case 0:
//      printf( "no fg\n" );
      break;

    case 1:
      fgcolor=argv[optind];
//      printf( "fg %s\n", fgcolor );
      break;

    default:
      check_locale_init();
      printf( _("%s: Too many arguments\n"), SET_COLOR );
      return 1;
  }

    /* Infer term256 support */
    char *fish_term256 = getenv("fish_term256");
    if (fish_term256) {
        support_term256 = from_string<bool>(fish_term256);
    } else {
        const char *term = getenv("TERM");
        support_term256 = term && strstr(term, "256color");
    }

  if( !fgcolor && !bgcolor && !bold && !underline )
  {
    check_locale_init();
    fprintf( stderr, _("%s: Expected an argument\n"), SET_COLOR );
    print_help( argv[0], 2 );
    return 1;
  }

    rgb_color_t fg = rgb_color_t(fgcolor ? fgcolor : "");
  if( fgcolor && fg.is_none())
  {
    check_locale_init();
    fprintf( stderr, _("%s: Unknown color '%s'\n"), SET_COLOR, fgcolor );
    return 1;
  }

    rgb_color_t bg = rgb_color_t(bgcolor ? bgcolor : "");
  if( bgcolor && bg.is_none())
  {
    check_locale_init();
    fprintf( stderr, _("%s: Unknown color '%s'\n"), SET_COLOR, bgcolor );
    return 1;
  }

  setupterm( 0, STDOUT_FILENO, 0);

  if( bold )
  {
    if( enter_bold_mode )
      putp( enter_bold_mode );
  }

  if( underline )
  {
    if( enter_underline_mode )
      putp( enter_underline_mode );
  }

  if( bgcolor )
  {
    if( bg.is_normal() )
    {
            write_background_color(0);
      putp( tparm(exit_attribute_mode) );
    }
  }

  if( fgcolor )
  {
    if( fg.is_normal() )
    {
            write_foreground_color(0);
      putp( tparm(exit_attribute_mode) );
    }
    else
    {
            write_foreground_color(index_for_color(fg));
    }
  }

  if( bgcolor )
  {
    if( ! bg.is_normal() )
    {
            write_background_color(index_for_color(bg));
    }
  }

  if( del_curterm( cur_term ) == ERR )
  {
    fprintf( stderr, "%s", _("Error while closing terminfo") );
  }

}
