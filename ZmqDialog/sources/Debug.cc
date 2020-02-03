#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "Debug.h"

namespace ZmqDialog
{
#define LIGHT_RED     "\e[31m\e[01m"
#define LIGHT_GREEN   "\e[32m\e[01m"
#define LIGHT_YELLOW  "\e[33m\e[01m"
#define LIGHT_BLUE    "\e[34m\e[01m"
#define LIGHT_MAGENTA "\e[35m\e[01m"
#define LIGHT_CYAN    "\e[36m\e[01m"
#define LIGHT_WHITE   "\e[37m\e[01m"
#define DARK_RED      "\e[31m"
#define DARK_GREEN    "\e[32m"
#define DARK_YELLOW   "\e[33m"
#define DARK_BLUE     "\e[34m"
#define DARK_MAGENTA  "\e[35m"
#define DARK_CYAN     "\e[36m"
#define DARK_WHITE    "\e[37m"

  int DLG_DEBUG_LEVEL = DBG_LEVEL_DEFAULT;

  void Print(int debug_level, const char* fmt, ...)
  {
    FILE* f = stdout;
    if(debug_level > DLG_DEBUG_LEVEL)
      return;
    if(debug_level == DBG_LEVEL_ERROR)
      f = stderr;
    const char* COLOR[DBG_LEVEL_DEBUG+1] =
      {
	LIGHT_RED,    // DBG_LEVEL_ERROR
	LIGHT_WHITE,  // DBG_LEVEL_INFO
	DARK_WHITE,   // DBG_LEVEL_DEFAULT
	LIGHT_BLUE,   // DBG_LEVEL_VERBOSE
	DARK_GREEN,   // 4
	DARK_YELLOW,  // 5
	DARK_RED,     // 6
	DARK_CYAN,    // 7
	DARK_MAGENTA, // 8
	DARK_GREEN,   // 9
	DARK_BLUE     // DBG_LEVEL_DEBUG
      };
    const char* END_OF_COLOR = "\e[0m";
    char msg[1024];
    va_list argptr;
    va_start(argptr,fmt);
    vsprintf(msg,fmt,argptr);
    va_end(argptr);
    bool addNewLineSymbol = false;
    if(msg[strlen(msg)-1] == '\n')
      {
	addNewLineSymbol = true;
	msg[strlen(msg)-1] = 0;
      }
    fprintf(f,"%s%s%s%c", COLOR[debug_level],msg,END_OF_COLOR,(addNewLineSymbol?'\n':'\0'));
  }

} // namespace ZmqDialog

