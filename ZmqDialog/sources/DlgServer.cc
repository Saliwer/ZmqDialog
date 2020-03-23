#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#include <zmq.hpp>

#include "DlgServer.h"
#include "Debug.h"
#include "Exception.h"
#include "Config.h"
#include "DlgMessage.h"

namespace ZmqDialog
{
  using namespace std;
  using namespace zmq;

#define HEARTBEAT_INTERVAL 2500000 // usecs
  
  int64_t current_time()
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t) (tv.tv_sec * 1000000 + tv.tv_usec);  
  }

  ////**********************************************************////
  ////                        ZMQ class                         ////
  ////**********************************************************////

  zmq::context_t* ZMQ::m_context  = nullptr;
  ZMQ*            ZMQ::m_instance = nullptr;

  ZMQ::ZMQ()
  {
    m_context  = new zmq::context_t(1);
  }

  ZMQ::~ZMQ()
  {
    delete m_context;
  }

  ZMQ* ZMQ::Instance()
  {
    if(!m_instance)
      {
	m_instance = new ZMQ;
      }
    return m_instance;
  }

  zmq::socket_t* ZMQ::CreateSocket(int socket_type)
  {
    return new zmq::socket_t(*m_context, socket_type);
  }


  ////**********************************************************////
  ////                   aService class                         ////
  ////**********************************************************////

  aService::aService(const char *name) : m_broker(nullptr)
  {
    m_name = name;
    m_broker = new aBroker(name);
  }
  
  aService::~aService()
  {
    delete m_broker;
  }

  bool aService::ReleaseMessage(DlgMessage* msg)
  {
    //........
    return true;
  }


  ////**********************************************************////
  ////                   aBroker  class                         ////
  ////**********************************************************////

  aBroker::aBroker(const char* name)
  {
    m_name =          name; 
    m_isRunning =     false;
    m_thread =        new std::thread(&aBroker::broker_thread, this);
    m_socket =        ZMQ::Instance()->CreateSocket(ZMQ_ROUTER);
    
    char port[256];
    size_t size = sizeof(port);
    m_socket->bind("tcp://192.168.0.112:0");
    // if(errno != 0 && errno != 11) // the resource can be temporary unavailable
    //   {
    // 	throw Exception("aBroker: socket binding error %d (%s)\n", errno, strerror(errno));
    //   }
    m_socket->getsockopt(ZMQ_LAST_ENDPOINT, &port, &size);
    m_port = std::string(port);

    Print(DBG_LEVEL_DEBUG,"aBroker: is bound to endpoint '%s'\n", m_port.c_str());
  }

  aBroker::~aBroker()
  {
    m_isRunning = false;
    if (m_thread && m_thread->joinable())
      m_thread->join();
    delete m_thread;
    
    //clear subscribers
    for(auto &sub : m_subscribers)
      delete sub.second;
    m_subscribers.clear();

    //clear publishers
    destroy_publishers();
    
    m_socket->close();
    delete m_socket;
  }

  void aBroker::broker_thread()
  {   
    Print(DBG_LEVEL_DEBUG,"Start of %s broker's thread\n", m_name.c_str());
    assert(this);
    m_isRunning = true;
    while(m_isRunning)
      {
	zmq::pollitem_t items[] = {
	  { static_cast<void*>(*m_socket), 0, ZMQ_POLLIN, 0 } 
	};
	
	int64_t timeout = 1000;  //usec
	
	//recieve messages
	zmq::poll(&items[0], 1, (long)timeout);
	if (items[0].revents & ZMQ_POLLIN)
	  {
	    DlgMessage* msg = new DlgMessage;
	    if(!msg->Recv(m_socket))
	      {
		Print(DBG_LEVEL_ERROR,"broker_thread: message receiving error.\n");
		delete msg;
		continue;
	      }
	    uint32_t msgType = 0;
	    if (!msg->GetMessageType(msgType))
	      {
		Print(DBG_LEVEL_ERROR,"broker_thread: bad message received (cannot get message type).\n");
		delete msg;
		continue;
	      }

	    //REGISTER_PUBLISHER_MESSAGE
	    if (msgType == REGISTER_PUBLISHER && !register_publisher(msg))
	      {
		Print(DBG_LEVEL_ERROR,"broker_thread: Couldn't register publisher.\n");
		delete msg;
		continue;	
	      }
	    
	    //SUBSCRIBE_TO_SERVICE_MESSAGE
	    if (msgType == SUBSCRIBE_TO_SERVICE && !subscribe_to_service(msg))
	      {
		Print(DBG_LEVEL_ERROR,"broker_thread: Couldn't subscribe to service.\n");
		delete msg;
		continue;
	      }	  
  
	    //PUBLISH_TEXT_MESSAGE
	    if (msgType == PUBLISH_TEXT_MESSAGE && !publish_text_message(msg))
	      {
	    	Print(DBG_LEVEL_ERROR,"broker_thread: Couldn't publish text message.\n");
	    	delete msg;
	    	continue;
	      }
	    
	    //PUBLISH_BINARY_MESSAGE
	    if (msgType == PUBLISH_BINARY_MESSAGE && !publish_binary_message(msg))
	      {
	    	Print(DBG_LEVEL_ERROR,"broker_thread: Couldn't publish binary message.\n");
	    	delete msg;
	    	continue;
	      }

	  }
	//send messages
	m_mutex.lock();
	for(std::map<std::string, aSubscriber*>::iterator it = m_subscribers.begin();
	    it != m_subscribers.end(); it++)
	  {
	    for(size_t i = 0; i < m_requests.size(); i++)
	      SendMessage(m_requests[i], it->second);
	  }
	for(size_t i = 0; i < m_requests.size(); i++)
	  delete m_requests[i];
	m_requests.clear();
	m_mutex.unlock();
      }
    Print(DBG_LEVEL_DEBUG,"End of %s broker thread\n", m_name.c_str());
    //    return nullptr;
  }

  bool aBroker::SendMessage(DlgMessage* msg, aSubscriber* s)
  {
    // std::string to;
    // if(!msg->GetToAddress(to))
    //   {
    // 	Print(DBG_LEVEL_ERROR,"aBroker::SendMessage: cannot get 'to' address of message.\n");
    // 	return false;
    //   }
    // if(!to.empty() && to != s->GetID())
    //   return true;
    if (!msg->SetIdentity(s->GetID()))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::SendMessage: cannot set identity address of message.\n");
     	return false;
      }
    return msg->Send(m_socket);
  }

  bool aBroker::AddRequest(DlgMessage* msg)
  {
    if(!msg)
      return false;
    m_mutex.lock();
    m_requests.push_back(msg);
    m_mutex.unlock();
    return true;
  }

  bool aBroker::AddSubscriber(const char *id)
  {
    std::string from(id);
    if (m_subscribers.count(from) != 0)
      {
	Print(DBG_LEVEL_DEBUG, "aBroker::AddSubscriber: this subscriber already exists '%s'\n", id);
	return false;
      }
    m_mutex.lock();
    m_subscribers[from] = new aSubscriber(id);
    m_mutex.unlock();
    return true;
  }

  bool aBroker::AddPublisher(const char* id)
  {
    std::string pub(id);
    if (m_publishers.count(pub) != 0)
      {
	Print(DBG_LEVEL_DEBUG, "aBroker::AddPublisher: this publisher already exists '%s'\n", id);
	return false;
      }
    m_mutex.lock();
    m_publishers[pub] = new aPublisher(id);
    m_mutex.unlock();
    return true;
  }

  void aBroker::destroy_publishers()
  {
    m_mutex.lock();
    for(auto &it : m_publishers)
      {
	delete it.second;
      }
    m_publishers.clear();
    m_mutex.unlock();
  }

  void aBroker::delete_publisher(const char* id)
  {
    m_mutex.lock();
    std::map<std::string, aPublisher*>::iterator it = m_publishers.find(id);
    if(it != m_publishers.end())
      {
	delete it->second;
	m_publishers.erase(it);
      }
    m_mutex.unlock();  
  }

  //Broker's handling messages
  
  bool aBroker::publish_text_message(DlgMessage *msg)
  {
    Print(DBG_LEVEL_DEBUG,"aBroker::publish_text_message: Broker %s get publish text message.\n", m_name.c_str());
    std::string identity;
    if(!msg->GetIdentity(identity))
      {
    	Print(DBG_LEVEL_ERROR,"aBroker::publish_text_message: Couldn't get identity.\n");
    	return false;
      }
    
    if (m_publishers.count(identity) == 0)
      {
	Print(DBG_LEVEL_DEBUG,"There are no any publishers for this message. You will be added as a publisher automatically.\n");
	if (!this->AddPublisher(identity.c_str()))
	  {
	    Print(DBG_LEVEL_ERROR,"aBroker::publish_text_message: Couldn't add publisher %s.\n", identity.c_str());
	    return false;
	  }
      }

    std::string msgBody;
    if(msg->GetMessageBody(msgBody))
      {
	Print(DBG_LEVEL_DEBUG,"<<%s>>\n", msgBody.c_str());
      }
    else
      {
	Print(DBG_LEVEL_ERROR,"aBroker::publish_text_message: bad message body received.\n");
	return false;
      }
       
    return this->AddRequest(msg);
  }

  bool aBroker::publish_binary_message(DlgMessage *msg)
  {
    std::string identity;
    if(!msg->GetIdentity(identity))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::publish_binary_message: Couldn't get identity.\n");
	return false;
      }
      
    if (m_publishers.count(identity) == 0)
      {
	Print(DBG_LEVEL_DEBUG,"There are no any publishers for this message. You will be added as a publisher automatically.\n");
	if (!this->AddPublisher(identity.c_str()))
	  {
	    Print(DBG_LEVEL_ERROR,"aBroker::publish_binary_message: Couldn't add publisher %s.\n", identity.c_str());
	    return false;
	  }
      }    

    void* buf = nullptr;
    size_t size = 0;
    if(msg->GetMessageBuffer(buf, size))
      {
	//Print(DBG_LEVEL_DEBUG,"binary message with size %ld\n", size);
      }
    else
      {
	Print(DBG_LEVEL_ERROR,"aBroker::publish_binary_message: bad binary message received.\n");
	return false;
      }
    
    if (buf)
      operator delete(buf);

    return this->AddRequest(msg);
  }


  bool aBroker::subscribe_to_service(DlgMessage *msg)
  { 
    std::string identity;
    if (!msg->GetIdentity(identity))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::subscribe_to_service: Couldn't get identity.\n");
	return false;
      }
    if (!this->AddSubscriber(identity.c_str()))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::register_publisher: Couldn't register subscriber %s.\n", identity.c_str());
	return false;
      }
    delete msg;
    return true;
  }

  bool aBroker::register_publisher(DlgMessage *msg)
  {
    std::string identity;
    if (!msg->GetIdentity(identity))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::register_publisher: Couldn't get identity.\n");
	return false;
      }
    
    if (!this->AddPublisher(identity.c_str()))
      {
	Print(DBG_LEVEL_ERROR,"aBroker::register_publisher: Couldn't register publisher %s.\n", identity.c_str());
	return false;	
      }
    delete msg;
    return true;
  }



  ////**********************************************************////
  ////                  DlgServer class                         ////
  ////**********************************************************////

  volatile bool DlgServer::m_isRunning = false;

  DlgServer::DlgServer() : m_router(nullptr), m_main_thread(nullptr)
  {
    try
      {
	m_router  = ZMQ::Instance()->CreateSocket(ZMQ_ROUTER);
	int socketID;
	size_t sizeSocketID = sizeof(socketID);
	m_router->getsockopt(ZMQ_TYPE, &socketID, &sizeSocketID);
	char endpoint[256];
    sprintf(endpoint,"tcp://192.168.0.112:%d",DLG_SERVER_TCP_PORT);
	m_router->bind(endpoint);
	size_t size = sizeof(endpoint);
	m_router->getsockopt(ZMQ_LAST_ENDPOINT, &endpoint, &size);
	Print(DBG_LEVEL_DEBUG, "The roter is bound with '%s'\n",endpoint);
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
    try
      {
	for (auto &v : m_services)
	  delete v.second;
	m_services.clear();
	if(m_main_thread && m_main_thread->joinable())
	  m_main_thread->join();
	Print(DBG_LEVEL_DEBUG, "*****\n");
	delete m_router;
	delete m_main_thread;
      }
    catch(std::exception e)
      {
	Print(DBG_LEVEL_DEBUG,"exception %s\n", e.what());
      }
    catch(...)
      {
	Print(DBG_LEVEL_DEBUG,"unknown exception\n");
      }
  }

  void DlgServer::Stop()
  {
    m_isRunning = false;
  }

  bool DlgServer::Start()
  {
    m_isRunning   = true;
    m_main_thread = new std::thread(&DlgServer::main_thread, this);
    Print(DBG_LEVEL_DEBUG,"Main thread is started.\n");
    return true;
  }

  bool DlgServer::create_service(const char* name)
  {
    if(m_services.count(name) != 0)
      return false;
    m_services[std::string(name)] = new aService(name);
    if(!m_services[std::string(name)])
      return false;
    return true;
  }



  void DlgServer::main_thread()
  {
    int64_t now = current_time();
    int64_t heartbeat_at = now + HEARTBEAT_INTERVAL;
    while(m_isRunning)
      {	
	zmq::pollitem_t items[] = {
	  { static_cast<void*>(*m_router), 0, ZMQ_POLLIN, 0} };
	int64_t timeout = heartbeat_at - now;
	if (timeout < 0)
	  timeout = 0;
	zmq::poll(&items[0], 1, 0);
	if (items[0].revents & ZMQ_POLLIN)
	  {
	    DlgMessage* msg = new DlgMessage;
	    if(!msg->Recv(m_router))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: message receiving error.\n");
		delete msg;
		continue;
	      }

	    std::string serviceName;
	    if(!msg->GetServiceName(serviceName))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: bad message received (cannot get service name).\n");
		delete msg;
		continue;
	      }
	    
	    if(m_services.count(serviceName) == 0 && !create_service(serviceName.c_str()))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: cannot create service '%s'\n", serviceName.c_str());
		delete msg;
		continue;
	      }
	    uint32_t msgType = 0;
	    if (!msg->GetMessageType(msgType))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: bad message received (cannot get message type).\n");
		delete msg;
		continue;
	      }
    	   
	    //SUBSCRIBE_TO_SERVICE_MESSAGE
	    if (msgType == SUBSCRIBE_TO_SERVICE && !subscribe_to_service(msg))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: Couldn't subscribe to service.\n");
		delete msg;
		continue;
	      }
    
	    //REGISTER_PUBLISHER_MESSAGE
	    if (msgType == REGISTER_PUBLISHER && !register_publisher(msg))
	      {
		Print(DBG_LEVEL_ERROR,"main_thread: Couldn't register publisher.\n");
		delete msg;
		continue;
	      }

	  }
      }
    m_isRunning = false;
    Print(DBG_LEVEL_DEBUG,"End of main thread.\n");
  }


  bool DlgServer::subscribe_to_service(DlgMessage *msg)
  {
    std::string serviceName;
    if(!msg->GetServiceName(serviceName))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::subscribe_to_service: bad message received (cannot get service name).\n");
	return false;
      }
    
    std::string identity;
    if (!msg->GetIdentity(identity))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::subscribe_to_service: Couldn't get identity.\n");
	return false;
      }

    if (!m_services[serviceName]->GetBroker()->AddSubscriber(identity.c_str()))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::subscribe_to_service: Couldn't add subscriber %s.\n", identity.c_str());
	return false;
      }
    
    std::string from("DlgServer");
    std::string brokerPort = m_services[serviceName]->GetBroker()->GetPort();
    DlgMessage *reply = new DlgMessage(serviceName, from, identity, SUBSCRIBE_TO_SERVICE, brokerPort);
    reply->SetIdentity(identity);
    if (!reply->Send(m_router))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::register_publisher: couldn't send a reply.\n");
	delete reply;
    	return false;
      }
    
    delete reply;
    delete msg;
    return true;
  }

  bool DlgServer::register_publisher(DlgMessage *msg)
  {    
    Print(DBG_LEVEL_DEBUG,"DlgServer::register_publisher message received.\n");
    std::string serviceName;
    if(!msg->GetServiceName(serviceName))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::register_publisher: bad message received (cannot get service name).\n");
	return false;
      }
    std::string identity;
    if (!msg->GetIdentity(identity))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::register_publisher: Couldn't get identity.\n");
	return false;
      }  

    if (!m_services[serviceName]->GetBroker()->AddPublisher(identity.c_str()))
      {
	Print(DBG_LEVEL_ERROR,"DlgServer::register_publisher: Couldn't register publisher %s.\n", identity.c_str());
	return false;
      }
    std::string from("DlgServer");
    std::string brokerPort = m_services[serviceName]->GetBroker()->GetPort();
    DlgMessage *reply = new DlgMessage(serviceName, from, identity, REGISTER_PUBLISHER, brokerPort);
    reply->SetIdentity(identity);

    if (!reply->Send(m_router))
      {
    	Print(DBG_LEVEL_ERROR,"DlgServer::register_publisher: couldn't send a reply.\n");
    	delete reply;
    	return false;
      }
    delete reply;
    delete msg;
    return true;
  }

} // namespace ZmqDialog
