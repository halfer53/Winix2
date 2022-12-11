#include <readline/readline.h>
#include <stddef.h>

char * rl_line_buffer = NULL; // This is the line gathered so far. You are welcome to modify the contents of the line, but see Allowing Undoing. The function rl_extend_line_buffer is available to increase the memory allocated to rl_line_buffer.
int rl_point = 0; // The offset of the current cursor position in rl_line_buffer (the point). This is always between 0 and rl_end.
int rl_end = 0; // The number of characters present in rl_line_buffer. When rl_point is at the end of the line, rl_point and rl_end are equal.
int rl_eof_found = 0; // Readline will set this variable when it has read an EOF character (e.g., the stty ‘EOF’ character) on an empty line or encountered a read error and is about to return a NULL line to the caller.
char* rl_prompt =  NULL; // The prompt Readline uses. This is set from the argument to readline(), and should not be assigned to directly. The rl_set_prompt() function (see Redisplay) may be used to modify the prompt string after calling readline().
int rl_done = 0; // This is a flag that is set to 1 when readline() has finished reading a line. It is useful for applications that want to call readline() multiple times in a loop, and do something else in between calls. The application should set this to 0 before calling readline() again.