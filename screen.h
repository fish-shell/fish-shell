/** \file screen.h High level library for handling the terminal screen

	The screen library allows the interactive reader to write its
	output to screen efficiently by keeping an inetrnal representation
	of the current screen contents and trying to find a reasonably
	efficient way for transforming that to the desired screen content.

	The current implementation is less smart than ncurses allows
	and can not for example move blocks of text around to handle text
	insertion.
  */
#ifndef FISH_SCREEN_H
#define FISH_SCREEN_H

/**
   The struct representing the current and desired screen contents.
*/
typedef struct
{
	/**
	  The internal representation of the desired screen contents.
	*/
	array_list_t desired;
	/**
	  The internal representation of the actual screen contents.
	*/
	array_list_t actual;
	/**
	   The desired cursor position.
	 */
	int desired_cursor[2];
	/**
	   The actual cursor position.
	*/
	int actual_cursor[2];
	/**
	   A stringbuffer containing the prompt which was last printed to
	   the screen.
	*/
	string_buffer_t actual_prompt;

	/**
	  The actual width of the screen at the time of the last screen
	  write.
	*/
	int actual_width;	

	/**
	   This flag is set to true when there is reason to suspect that
	   the parts of the screen lines where the actual content is not
	   filled in may be non-empty. This means that a clr_eol command
	   has to be sent to the terminal at the end of each line.
	*/
	int need_clear;
	
	/**
	   These status buffers are used to check if any output has occurred
	   other than from fish's main loop, in which case we need to redraw.
	*/
	struct stat prev_buff_1, prev_buff_2, post_buff_1, post_buff_2;
}
	screen_t;

/**
   A struct representing a single line of a screen. Consists of two
   array_lists, which must always be of the same length.
*/
typedef struct
{
	/**
	   The text contents of the line. Each element of the array
	   represents on column of output. Because some characters are two
	   columns wide, it is perfectly possible for some of the comumns
	   to be empty. 
	*/
	array_list_t text;
	/**
	   Highlight information for the line
	*/
	array_list_t color;
}
	line_t;

/**
   Initialize a new screen struct
*/
void s_init( screen_t *s );

/**
   Free all memory used by the specified screen struct
*/
void s_destroy( screen_t *s );

/**
   This is the main function for the screen putput library. It is used
   to define the desired contents of the screen. The screen command
   will use it's knowlege of the current contents of the screen in
   order to render the desired output using as few terminal commands
   as possible.
*/
void s_write( screen_t *s, 
			  wchar_t *prompt, 
			  wchar_t *commandline,
			  int *colors, 
			  int *indent,
			  int cursor_pos );

/** 
    This function resets the screen buffers internal knowledge about
    the contents of the screen. Use this function when some other
    function than s_write has written to the screen.

    \param s the screen to reset
    \param reset_cursor whether the line on which the curor has changed should be assumed to have changed. If \c reset_cursor is set to 0, the library will attempt to make sure that the screen area does not seem to move up or down on repaint. 

    If reset_cursor is incorreclt set to 0, this may result in screen
    contents being erased. If it is incorrectly set to one, it may
    result in one or more lines of garbage on screen on the next
    repaint. If this happens during a loop, such as an interactive
    resizing, there will be one line of garbage for every repaint,
    which will quicly fill the screen.
*/
void s_reset( screen_t *s, int reset_cursor );

#endif
