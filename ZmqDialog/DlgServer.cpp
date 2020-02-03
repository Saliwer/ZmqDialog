#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/time.h>

#include <thread>
#include <string>
#include <deque>
#include <map>

#include <zmq.hpp>

#include "ZmqDialog.h"
#include "Debug.h"
#include "Exception.h"
#include "Config.h"
#include "DlgMessage.h"

namespace ZmqDialog
{
  volatile bool IS_INTERRUPTED = false;
}

using namespace std;
using namespace zmq;
using namespace ZmqDialog;

int64_t current_time()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t) (tv.tv_sec * 1000000 + tv.tv_usec);  
}

struct aPublisher;

struct aSubscriber
{
  std::string             m_id;
  int64_t                 m_expiry;    //  Expiries at unless heartbeat
  aSubscriber(const char* id, int64_t expiry = 0) : m_expiry(expiry)
  {
    m_id = id;
  }
};

struct aPublisher
{
  std::string             m_id;
  int64_t                 m_expiry;    //  Expiries at unless heartbeat
  aPublisher(const char* id, int64_t expiry = 0) : m_expiry(expiry)
  {
    m_id = id;
  }
};

struct aBroker;

struct aService
{
  std::string                         m_name;
  std::map<std::string, aPublisher*>  m_publishers;
  aBroker*                            m_broker;
  aService(const char* name) : m_broker(nullptr)
  {
    m_name = name;
  }
};

struct aBroker
{
  std::string                         m_name;
  std::map<std::string, aSubscriber*> m_subscribers;
  std::deque<DlgMessage*>             m_requests;
  aBroker(const char* name)
  {
    m_name = name;
  }
};

class DlgServer
{
  zmq::context_t* m_context;
  zmq::socket_t*  m_router;
  std::thread*    m_main_thread;

protected:
  std::map<std::string, aService*>    m_services;
  std::map<std::string, aPublisher*>  m_publishers;
  std::map<std::string, aSubscriber*> m_subscribers;
  std::map<std::string, aBroker*>     m_brokers;
public:
  DlgServer();
  ~DlgServer();
  
  bool Start();
  void Stop();

  bool PropagateMessage(DlgMessage);
private:
  void main_thread();

  bool create_service(const char* name);
  bool create_publisher(const char* name);
};

DlgServer::DlgServer() : m_context(nullptr), m_router(nullptr), m_main_thread(nullptr)
{
  try
    {
      m_context = new zmq::context_t(1);
      m_router  = new zmq::socket_t(*m_context, ZMQ_ROUTER);
      char address[256];
      sprintf(address,"tcp://*:%d",DLG_SERVER_TCP_PORT);
      m_router->bind(address);
      Print(DBG_LEVEL_DEBUG, "The roter is bound with '%s'\n",address);
    }
  catch(zmq::error_t& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgServer() zmq::exception %s\n", e.what());
      throw Exception("DlgServer() fatal error.");
    }
  catch(std::exception& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgServer() std::exception %s\n", e.what());
      throw Exception("DlgServer() fatal error.");
    }
  catch(...)
    {
      Print(DBG_LEVEL_ERROR, "DlgServer() unknown exeption.\n");
      throw Exception("DlgServer() fatal error.");
    }
}

DlgServer::~DlgServer()
{
  Stop();
  if(m_main_thread)
    m_main_thread->join();
  delete m_router;
  delete m_context;
  delete m_main_thread;
}

#define HEARTBEAT_INTERVAL 2500000 // usecs

void DlgServer::main_thread()
{
  int64_t now = current_time();
  int64_t heartbeat_at = now + HEARTBEAT_INTERVAL;
  while(!IS_INTERRUPTED)
    {
      zmq::pollitem_t items[] = {
	{ static_cast<void*>(*m_router), 0, ZMQ_POLLIN, 0} };
      int64_t timeout = heartbeat_at - now;
      if (timeout < 0)
	timeout = 0;
      zmq::poll(items, 1, (long)timeout);
      if (items[0].revents & ZMQ_POLLIN)
	{
	  DlgMessage* msg = new DlgMessage;
	  if(!msg->Recv(m_router))
	    {
	      Print(DBG_LEVEL_ERROR,"main_thread: message receiving error.\n");
	      break;
	    }
	  std::string serviceName;
	  if(!msg->GetFromAddress(serviceName))
	    {
	      Print(DBG_LEVEL_ERROR,"main_thread: bad message received (cannot get service name).\n");
	      delete msg;
	      break;
	    }
	  else
	    {
	      Print(DBG_LEVEL_DEBUG,"Message service name: '%s'\n", serviceName.c_str());
	    }
	  if(m_services.count(serviceName) == 0 && !create_service(serviceName))
	    {
	      Print(DBG_LEVEL_ERROR,"main_thread: cannot create service '%s'\n", serviceName.c_str());
	      delete msg;
	      break;
	    }
	  uint32_t msgType = 0;
	  if(!msg->GetMessageType(msgType))
	    {
	      Print(DBG_LEVEL_ERROR,"main_thread: bad message received (cannot get message type).\n");
	      delete msg;
	      break;
	    }
	  else
	    {
	      Print(DBG_LEVEL_DEBUG,"Message type: %u\n", msgType);
	    }
	  if(msgType == PUBLISH_TEXT_MESSAGE || msgType == PUBLISH_BINARY_MESSAGE)
	    {
	      aService service* = m_services.at(serviceName);
	      std::string from;
	      if(m_publishers.count(from) == 0 && !create_publisher(from.c_str()))
		{
		  Print(DBG_LEVEL_ERROR,"main_thread: cannot create publisher '%s'\n", from.c_str());
		  delete msg;
		  break;
		}
	      if(m_brokers.count(serviceName) == 0 && !create_broker(serviceName.c_str()))
		{
		  Print(DBG_LEVEL_ERROR,"main_thread: cannot create broker '%s'\n", serviceName.c_str());
		  delete msg;
		  break;
		}
	      aBroker* broker = m_brokers.at(serviceName);
	      aPublisher* publisher = m_publishers.at(from);
	      if(service->m_publishers.count(from) == 0)
		service->m_publishers[from] = publisher;
	      if(service->m_broker == NULL)
		service->m_broker = broker;
	      if(msgType == PUBLISH_TEXT_MESSAGE)
		{
		  std::string msgBody;
		  if(msg->GetMessageBody(msgBody))
		    {
		      Print(DBG_LEVEL_DEBUG,"<<%s>>\n", msgBody.c_str());
		      broker->m_requests.push_back(msg);
		    }
		  else
		    {
		      Print(DBG_LEVEL_ERROR,"main_thread: bad message body received.\n");
		      delete msg;
		      break;
		    }
		}
	      else //  msgType == PUBLISH_BINARY_MESSAGE
		{
		  void* buf = NULL;
		  size_t size = 0;
		  if(msg->GetMessageBuffer(buf,size))
		    {
		      Print(DBG_LEVEL_DEBUG,"binary message with size %ld\n", size);
		      broker->m_requests.push_back(msg);
		    }
		  else
		    {
		      Print(DBG_LEVEL_ERROR,"main_thread: bad binary message body received.\n");
		      delete msg;
		      break;
		    }
		}
	    }
	  // if (header.compare(MDPC_CLIENT) == 0)
	  //   {
	  //     _this->client_process(sender, msg);
	  //   }
	  // else if (header.compare(MDPW_WORKER) == 0)
	  //   {
	  //     _this->subscriber_process(sender, msg);
	  //   }
	  // else
	  //   {
	  //     s_console ("E: invalid message:");
	  //     msg->dump();
	  //     delete msg;
	  //   }
	}
      //  Disconnect and delete any expired subscribers
      //  Send heartbeats to idle subscribers if needed
      now = current_time();
      if (now >= heartbeat_at)
	{
	  usleep(1000);
	  // _this->purge_subscribers();
	  // for (std::set<aSubscriber*>::iterator it = _this->m_waiting.begin();
	  //      it != _this->m_waiting.end() && (*it)!=0; it++)
	  //   {
	  //     _this->send_to_subscriber(*it, (char*)MDPW_HEARTBEAT, "", NULL);
	  //   }
	  heartbeat_at += HEARTBEAT_INTERVAL;
	  now = current_time();
	}
    }
  IS_INTERRUPTED = true;
  Print(DBG_LEVEL_DEBUG,"End of main thread.\n");
}

void DlgServer::Stop()
{
  IS_INTERRUPTED = true;
}

bool DlgServer::Start()
{
  m_main_thread = new std::thread(&DlgServer::main_thread,this);
  Print(DBG_LEVEL_DEBUG,"Main thread is started.\n");
}

bool DlgServer::PropagateMessage(DlgMessage)
{
  std::string from;
  std::string to;
  uint32_t    msgType = 0;
  std::string msgBody;
  msg.GetFromAddress(from);
  if(!m_bro)
    {
      m_publishers[name] = new 
    }
}

bool DlgServer::create_service(const char* name)
{
  if(m_services.count(name) != 0)
    return false;
  m_services[std::string(name)] = new aService(name);
  return true;
}

bool DlgServer::create_publisher(const char* name)
{
  if(m_publishers.count(name) != 0)
    return false;
  m_publishers[std::string(name)] = new aPublishers(name);
  return true;
}

bool DlgServer::create_broker(

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
	  IS_INTERRUPTED = true;
	  Print(DBG_LEVEL_DEBUG,"Command \'%s\' is received.\n",line);
	  break;
	}
      free(line);
    }
  write_history(history_file_name);
  return 0;
}
