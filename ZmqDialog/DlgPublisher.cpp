#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/time.h>


#include <zmq.hpp>

#include "DlgServer.h"
#include "DlgPublisher.h"
#include "Config.h"
#include "Debug.h"
#include "ZmqDialog.h"
#include "DlgMessage.h"
#include "Exception.h"

#include <ctime>


#include <random>


using namespace ZmqDialog;

void USAGE(int argc, char* argv[])
{
  printf("%s <option>\n", argv[0]);
  printf("    where possible option(s) are:\n");
  printf("    -d          - debug mode (all printouts)\n");
  printf("    -v          - verbose mode\n");
  printf("    -s          - silent mode (minimum printout)\n");
}

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


  DlgPublisher Publisher("Publisher #1", "SomeService");
  if (!Publisher.Register())
    {
      Print(DBG_LEVEL_ERROR,"Couldn't register publisher.\n");
    }

  
  char* line = NULL;

  
  while((line = readline("Publisher> ")) != NULL)
    {
      if(strncmp(line,"exit",4) == 0 || strncmp(line,"quit",4) == 0)
	{
	  Print(DBG_LEVEL_DEBUG,"Command \'%s\' is received.\n",line);
	  break;
	}
      if (strncmp(line, "publish", 7) == 0)
	{
	  for(size_t i = 0; i < 1000; ++i)
	    {
	      DlgMessage msg;
	      if (!msg.SetMessageType(PUBLISH_BINARY_MESSAGE))
		{
		  Print(DBG_LEVEL_ERROR,"Couldn't set message type.\n");
		  continue;
		}
	      timeval current_time;
	      if (gettimeofday(&current_time, NULL) != 0)
		{
		  Print(DBG_LEVEL_DEBUG, "Get time of day error\n");
		  continue;
		}
	      	
	      if (!msg.SetMessageBuffer(&current_time, sizeof(current_time)))
		{
		  Print(DBG_LEVEL_ERROR,"Couldn't set message body.\n");
		  continue;
		}
	      if (!Publisher.PublishMessage(&msg))
		{
		  Print(DBG_LEVEL_ERROR,"Couldn't publish message.\n");
		  continue;
		}
	       usleep(1000);
	    }	  
	}
      free(line);
    }
  
  Print(DBG_LEVEL_DEBUG,"End of program.\n");  
  return 0;
}
