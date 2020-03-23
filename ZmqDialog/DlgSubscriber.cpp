#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/time.h>

#include <zmq.hpp>

#include "DlgServer.h"
#include "DlgSubscriber.h"
#include "Config.h"
#include "Debug.h"
#include "ZmqDialog.h"
#include "DlgMessage.h"
#include "Exception.h"

#include <ctime>


void USAGE(int argc, char* argv[])
{
  printf("%s <option>\n", argv[0]);
  printf("    where possible option(s) are:\n");
  printf("    -d          - debug mode (all printouts)\n");
  printf("    -v          - verbose mode\n");
  printf("    -s          - silent mode (minimum printout)\n");
}


static bool thread_run = false;
const char *server_name = "192.168.0.112:55550";

void some_loop(void*);


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

  Print(DBG_LEVEL_DEBUG, "Dlg Subscriber is starting with option '%s'...\n",argv[1]);

  DlgSubscriber tempSub("Subscriber #1", "SomeService");
  if (!tempSub.Connect(server_name))
    {
      Print(DBG_LEVEL_ERROR,"Couldn't connect to server.\n");
    }
  if (!tempSub.Subscribe())
    {
      Print(DBG_LEVEL_ERROR,"Couldn't subscribe to server.\n");
    }
  thread_run = true;

  std::thread* subthread = new std::thread(&some_loop, &tempSub);

  char* line = NULL;
  while((line = readline("Subscriber> ")) != NULL)
    {
      if(strncmp(line,"exit",4) == 0 || strncmp(line,"quit",4) == 0)
	{
	  thread_run = false;
	  Print(DBG_LEVEL_DEBUG,"Command \'%s\' is received.\n",line);
	  break;
	}
      free(line);
    }
  subthread->join();
  delete subthread;
  Print(DBG_LEVEL_DEBUG,"End of program.\n");
  return 0;
}

void some_loop(void *param)
{	    
  Print(DBG_LEVEL_DEBUG, "Start of some_loop function\n");
  DlgSubscriber *sub = static_cast<DlgSubscriber*>(param);
  while(thread_run)
    {
      if (sub->HasData())
	{
	  DlgMessage *msg = nullptr;
	  if (!sub->GetMessage(msg))
	    {
	      Print(DBG_LEVEL_DEBUG, "Couldn't get message from subscriber\n");
	      continue;
	    }

	  timeval* receive_time = nullptr;
	  void *buf = nullptr;
	  size_t size = 0;
	  if (msg->GetMessageBuffer(buf, size))
	    {
	      Print(DBG_LEVEL_DEBUG, "Binary message received with size %u.\n", size);
	      buf = operator new(size);
	      if (msg->GetMessageBuffer(buf, size))
		{
		  receive_time = static_cast<timeval*>(buf);
		}
	      else
		{
		  Print(DBG_LEVEL_DEBUG, "Couldn't get binary data.\n");
		  operator delete(buf);
		  continue;
		}
	    }
	  else
	    {
	      Print(DBG_LEVEL_DEBUG, "Couldn't get binary message.\n");
	      continue;
	    }
	 timeval current_time;
	  if (gettimeofday(&current_time, NULL) != 0)
	    {
	      Print(DBG_LEVEL_DEBUG, "Get time of day error\n");
	      operator delete(buf);
	      continue;
	    }
	  timeval res;
	  timersub(&current_time, receive_time, &res);
	  Print(DBG_LEVEL_DEBUG,"Time elapsed: %lus   %luus\n", res.tv_sec, res.tv_usec);
	  operator delete(buf);
	}
    } 
  Print(DBG_LEVEL_DEBUG, "End of some_loop function\n");
}
