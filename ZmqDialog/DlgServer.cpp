#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/time.h>

#include <zmq.hpp>

#include "DlgServer.h"
#include "Config.h"
#include "Debug.h"
#include "ZmqDialog.h"

using namespace std;
using namespace ZmqDialog;

void USAGE(int argc, char* argv[])
{
  printf("%s <option>\n", argv[0]);
  printf("    where possible option(s) are:\n");
  printf("    -d          - debug mode (all printouts)\n");
  printf("    -v          - verbose mode\n");
  printf("    -s          - silent mode (minimum printout)\n");
}

static const char* history_file_name = ".server.history";

int main(int argc, char* argv[])
{
  DLG_DEBUG_LEVEL = DBG_LEVEL_DEFAULT;
  if(argc != 2)
    {
      USAGE(argc,argv);
      return 1;
    }
  int c = 0;
  while((c = getopt(argc,argv,"vsd")) != -1)
    {
      switch (c)
	{
	case 'v':
	  DLG_DEBUG_LEVEL = DBG_LEVEL_VERBOSE;
	  break;
	case 's':
	  DLG_DEBUG_LEVEL = DBG_LEVEL_ERROR;
	  break;
	case 'd':
	  DLG_DEBUG_LEVEL = DBG_LEVEL_DEBUG;
	  break;	  
	default:
	  fprintf(stderr,"Unknown option '-%c'.\n", optopt);
	case '?':
	  USAGE(argc,argv);
	  return 1;
	}
    }
  Print(DBG_LEVEL_DEBUG, "Dlg Server is starting with option '%s'...\n",argv[1]);
  DlgServer server;
  server.Start();
  read_history(history_file_name);
  char* line = NULL;
  while((line = readline("server> ")) != NULL)
    {
      add_history(line);
      if(strncmp(line,"exit",4) == 0 || strncmp(line,"quit",4) == 0)
	{
	  server.Stop();
	  Print(DBG_LEVEL_DEBUG,"Command \'%s\' is received.\n",line);
	  break;
	}
      free(line);
    }
  write_history(history_file_name);
  Print(DBG_LEVEL_DEBUG,"End of program.\n");
  return 0;
}
