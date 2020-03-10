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

static bool publish = false;
static bool run_threads = false;

void norm_distr();
void exp_distr();
void gaus_distr();


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

  run_threads = true;
  std::thread* norm_distr_thread = new std::thread(&norm_distr);
  std::thread* gaus_distr_thread = new std::thread(&gaus_distr);
  std::thread* exp_distr_thread = new std::thread(&exp_distr);

  char* line = NULL;
  while((line = readline("Publisher> ")) != NULL)
    {
      if(strncmp(line,"exit",4) == 0 || strncmp(line,"quit",4) == 0)
	{
	  run_threads = false;
	  Print(DBG_LEVEL_DEBUG,"Command \'%s\' is received.\n",line);
	  break;
	}
      if (strncmp(line, "publish", 7) == 0)
	{
	  publish = true;	  
	}
      free(line);
    }
  
  norm_distr_thread->join();
  delete norm_distr_thread;
  gaus_distr_thread->join();
  delete gaus_distr_thread;
  exp_distr_thread->join();
  delete exp_distr_thread;

  Print(DBG_LEVEL_DEBUG,"End of program.\n");
   
  return 0;
}

void norm_distr()
{      
  Print(DBG_LEVEL_DEBUG,"Start of real_distr publisher thread.\n");
  DlgPublisher Publisher("Norm_distr publisher ", "NORM");
  if (!Publisher.Register())
    {
      Print(DBG_LEVEL_ERROR,"Couldn't register publisher.\n");
    }

  DlgMessage msg;
  if (!msg.SetMessageType(PUBLISH_BINARY_MESSAGE))
    {
      Print(DBG_LEVEL_ERROR,"Couldn't set message type.\n");
    }
 
 while(run_threads)
    {
      if(publish)
	{
	  uint32_t value = 0;
	  value = rand()%1000;
	  if (!msg.SetMessageBuffer(&value, sizeof(value)))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't set message body.\n");
	    }
	  
	  if (!Publisher.PublishMessage(&msg))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't publish message.\n");
	    }
	}
      usleep(10000);
    }
   Print(DBG_LEVEL_DEBUG,"End of real_distr publisher thread.\n");
}


void exp_distr()
{   
  Print(DBG_LEVEL_DEBUG,"Start of exp_distr publisher thread.\n");
  DlgPublisher Publisher("Exp_distr publisher", "EXP");
  if (!Publisher.Register())
    {
      Print(DBG_LEVEL_ERROR,"Couldn't register publisher.\n");
    }

  DlgMessage msg;
  if (!msg.SetMessageType(PUBLISH_BINARY_MESSAGE))
    {
      Print(DBG_LEVEL_ERROR,"Couldn't set message type.\n");
    }
  while(run_threads)
    {
      if(publish)
	{
	  uint32_t value = 0;
	  std::random_device rd;
	  std::mt19937 gen(rd());
	  
	  std::exponential_distribution<> dist(1);
	  value = dist(gen) * 100;
	  if (!msg.SetMessageBuffer(&value, sizeof(value)))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't set message body.\n");
	    }
	  
	  if (!Publisher.PublishMessage(&msg))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't publish message.\n");
	    }
	}
      usleep(10000);
    }   
  Print(DBG_LEVEL_DEBUG,"End of real_distr publisher thread.\n");
}




void gaus_distr()
{   
  Print(DBG_LEVEL_DEBUG,"Start of norm_distr publisher thread.\n");
  DlgPublisher Publisher("Gaus_distr publisher", "GAUS");
  if (!Publisher.Register())
    {
      Print(DBG_LEVEL_ERROR,"Couldn't register publisher.\n");
    }

  DlgMessage msg;
  if (!msg.SetMessageType(PUBLISH_BINARY_MESSAGE))
    {
      Print(DBG_LEVEL_ERROR,"Couldn't set message type.\n");
    }
  
  while(run_threads)
    {
      if(publish)
	{
	  uint32_t value = 0;
	  std::random_device rd;
	  std::mt19937 gen(rd());
	  
	  std::normal_distribution<> dist(500,100);
	  value = dist(gen);
	  if (!msg.SetMessageBuffer(&value, sizeof(value)))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't set message body.\n");
	    }
	  
	  if (!Publisher.PublishMessage(&msg))
	    {
	      Print(DBG_LEVEL_ERROR,"Couldn't publish message.\n");
	    }
	}
      usleep(10000);
    }  
  Print(DBG_LEVEL_DEBUG,"End of norm_distr publisher thread.\n");
}
