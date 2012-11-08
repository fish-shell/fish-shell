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

#include <vector>

/**
   A class representing a single line of a screen.
*/
struct line_t
{
    std::vector<wchar_t> text;
    std::vector<int> colors;
    bool is_soft_wrapped;
    
    line_t() : text(), colors(), is_soft_wrapped(false)
    {
    }
    
    void clear(void)
    {
        text.clear();
        colors.clear();
    }
    
    void append(wchar_t txt, int color)
    {
        text.push_back(txt);
        colors.push_back(color);
    }
        
    size_t size(void) const
    {
        return text.size();
    }
    
    wchar_t char_at(size_t idx) const
    {
        return text.at(idx);
    }
    
    int color_at(size_t idx) const
    {
        return colors.at(idx);
    }
    
};

/**
 A class representing screen contents.
*/
class screen_data_t
{
    std::vector<line_t> line_datas;
    
    public:
    
    struct cursor_t {
        int x;
        int y;
        cursor_t() : x(0), y(0) { }
        cursor_t(int a, int b) : x(a), y(b) { }
    } cursor;
    
    line_t &add_line(void) {
        line_datas.resize(line_datas.size() + 1);
        return line_datas.back();
    }
    
    void resize(size_t size) {
        line_datas.resize(size);
    }
        
    line_t &create_line(size_t idx) {
        if (idx >= line_datas.size()) {
            line_datas.resize(idx + 1);
        }
        return line_datas.at(idx);
    }
    
    line_t &line(size_t idx) {
        return line_datas.at(idx);
    }
    
    size_t line_count(void) {
        return line_datas.size();
    }
};

/**
   The class representing the current and desired screen contents.
*/
class screen_t
{
    public:

    /** Constructor */
    screen_t();

    /**
      The internal representation of the desired screen contents.
    */
    screen_data_t desired;
    /**
      The internal representation of the actual screen contents.
    */
    screen_data_t actual;

    /**
       A string containing the prompt which was last printed to
       the screen.
    */
    wcstring actual_left_prompt;
    
    /** Last right prompt width */
    size_t last_right_prompt_width;

    /**
      The actual width of the screen at the time of the last screen
      write.
    */
    int actual_width;
    
    /** If we support soft wrapping, we can output to this location without any cursor motion. */
    screen_data_t::cursor_t soft_wrap_location;

    /**
	   This flag is set to true when there is reason to suspect that
	   the parts of the screen lines where the actual content is not
	   filled in may be non-empty. This means that a clr_eol command
	   has to be sent to the terminal at the end of each line.
	*/
	bool need_clear;
        
    /** If we need to clear, this is how many lines the actual screen had, before we reset it. This is used when resizing the window larger: if the cursor jumps to the line above, we need to remember to clear the subsequent lines. */
    size_t actual_lines_before_reset;
	
	/**
	   These status buffers are used to check if any output has occurred
	   other than from fish's main loop, in which case we need to redraw.
	*/
	struct stat prev_buff_1, prev_buff_2, post_buff_1, post_buff_2;
};

/**
   This is the main function for the screen putput library. It is used
   to define the desired contents of the screen. The screen command
   will use it's knowlege of the current contents of the screen in
   order to render the desired output using as few terminal commands
   as possible.
   
    \param s the screen on which to write
    \param left_prompt the prompt to prepend to the command line
    \param right_prompt the right prompt, or NULL if none
    \param commandline the command line
    \param explicit_len the number of characters of the "explicit" (non-autosuggestion) portion of the command line
    \param colors the colors to use for the comand line
    \param indent the indent to use for the command line
    \param cursor_pos where the cursor is
*/
void s_write( screen_t *s, 
			  const wchar_t *left_prompt,
			  const wchar_t *right_prompt,
			  const wchar_t *commandline,
			  size_t explicit_len,
			  const int *colors, 
			  const int *indent,
			  size_t cursor_pos );


void s_write( screen_t *s,
              const wcstring &left_prompt,
              const wcstring &right_prompt,
              const wcstring &commandline,
              size_t explicit_len,
              const int *colors,
              const int *indent,
              size_t cursor_pos );

/** 
    This function resets the screen buffers internal knowledge about
    the contents of the screen. Use this function when some other
    function than s_write has written to the screen.

    \param s the screen to reset
    \param reset_cursor whether the line on which the curor has changed should be assumed to have changed. If \c reset_cursor is false, the library will attempt to make sure that the screen area does not seem to move up or down on repaint.
    \param Whether to reset the prompt as well. If so

    If reset_cursor is incorreclt set to 0, this may result in screen
    contents being erased. If it is incorrectly set to one, it may
    result in one or more lines of garbage on screen on the next
    repaint. If this happens during a loop, such as an interactive
    resizing, there will be one line of garbage for every repaint,
    which will quicly fill the screen.
*/
void s_reset( screen_t *s, bool reset_cursor, bool reset_prompt = true );

#endif
