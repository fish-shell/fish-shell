#ifndef FISH_SCREEN_H
#define FISH_SCREEN_H

#define SCREEN_REPAINT 1
#define SCREEN_SKIP_RETURN 2

typedef struct
{
	array_list_t output;
	array_list_t screen;
	int output_cursor[2];
	int screen_cursor[2];
	string_buffer_t prompt_buff;
}
	screen_t;

typedef struct
{
	array_list_t text;
	array_list_t color;
}
	line_t;

void s_init( screen_t *s );
void s_destroy( screen_t *s );

void s_write( screen_t *s, 
			  wchar_t *prompt, 
			  wchar_t *b,
			  int *c, 
			  int cursor );
void s_reset( screen_t *s );

#endif
