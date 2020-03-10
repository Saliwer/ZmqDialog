#include "DlgPublisher.h"

////**********************************************************////
////                  DlgPublisher class                      ////
////**********************************************************////

DlgPublisher::DlgPublisher(const std::string &name, const std::string &service) : 
  m_name(name), m_service(service), m_socket(nullptr)
{}


DlgPublisher::~DlgPublisher()
{
  m_socket->close();
  delete m_socket;
}

bool DlgPublisher::Register()
{
  m_socket = ZMQ::Instance()->CreateSocket(ZMQ_DEALER);
  m_socket->setsockopt(ZMQ_IDENTITY, m_name.c_str(), m_name.size() + 1);
  char endpoint[256];
  try
    {
      sprintf(endpoint, "tcp://192.168.100.13:%d", DLG_SERVER_TCP_PORT);
      m_socket->connect(endpoint);
    }
  catch(zmq::error_t& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register() zmq::exception %s\n", e.what());
      throw Exception("DlgPublisher::Register() fatal error.");
    }
  catch(std::exception& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register() std::exception %s\n", e.what());
      throw Exception("DlgPublisher::Register() fatal error.");
    }
  catch(...)
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register() unknown exeption.\n");
      throw Exception("DlgPublisher::Register() fatal error.");
    }
 
  Print(DBG_LEVEL_DEBUG, "Publisher %s connected to %s\n",m_name.c_str() ,endpoint);
  
  std::string to("");
  std::string msgBody("");
  DlgMessage *msg = new DlgMessage(m_service, m_name, to, REGISTER_PUBLISHER, msgBody);
  msg->SetIdentity(m_name);


  if (!msg->Send(m_socket))
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register() : Couldn't send message to DlgServer");
      delete msg;
      return false;
    }
  
  zmq::pollitem_t items[] = {
    {static_cast<void*>(*m_socket), 0, ZMQ_POLLIN, 0}
  };

  int cnt = 0;
  while(++cnt < 4 )
    {
      zmq::poll(&items[0], 1, (long)TIMEOUT_INTERVAL/1000);
      if(items[0].revents & ZMQ_POLLIN)
	{

	  if (!msg->Recv(m_socket))
	    {
	      Print(DBG_LEVEL_ERROR,"DlgPublisher::Register(): message receiving error.\n");
	      delete msg;
	      return false;
	    }
	  
	  uint32_t msgType = 0;
	  if (!msg->GetMessageType(msgType))
	    {
	      Print(DBG_LEVEL_ERROR,"DlgPublisher::Register(): bad message received(cannot get message type).\n");
	      delete msg;
	      return false;
	    }
	  
	  if (msgType != REGISTER_PUBLISHER)
	    {
	      Print(DBG_LEVEL_ERROR,"DlgPublisher::Register(): incorrect reply from server.\n");
	      delete msg;
	      return false;
	    }
	  
	  std::string brokerPort;
	  if(!msg->GetMessageBody(brokerPort))
	    {
	      Print(DBG_LEVEL_ERROR,"DlgPublisher::Register(): couldn't get message body.\n");
	      delete msg;
	      return false; 
	    }
	  
	  Print(DBG_LEVEL_DEBUG,"DlgPublisher::Register(): Get broker's port : '%s'.\n", brokerPort.c_str());
	  Print(DBG_LEVEL_DEBUG,"DlgPublisher::Register(): Trying reconnect to the new port.\n");
	  
	 
	  m_socket->close();
	  
	  m_socket = ZMQ::Instance()->CreateSocket(ZMQ_DEALER);
	  m_socket->setsockopt(ZMQ_IDENTITY, m_name.c_str(), m_name.size() + 1);
	  try
	    {
	      m_socket->connect(brokerPort.c_str());
	    }
	  catch(zmq::error_t& e)
	    {
	      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register(): zmq::exception %s\n", e.what());
	      throw Exception("DlgPublisher::Register(): fatal error.");
	    }
	  catch(std::exception& e)
	    {
	      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register(): std::exception %s\n", e.what());
	      throw Exception("DlgPublisher::Register(): fatal error.");
	    }
	  catch(...)
	    {
	      Print(DBG_LEVEL_ERROR, "DlgPublisher::Register(): unknown exeption.\n");
	      throw Exception("DlgPublisher::Register(): fatal error.");
	    }
	  
	  Print(DBG_LEVEL_DEBUG, "DlgPublisher::Register(): Publisher %s connected to port: %s\n",m_name.c_str() , brokerPort.c_str()); 	  
	 	  	  
	  cnt = 4;
	}
      else
	{
	  Print(DBG_LEVEL_DEBUG, "DlgPublishe::Register(): Couldn't get a response from server %d \n", cnt);
	}
    }
  delete msg;
  return true;
}

bool DlgPublisher::PublishMessage(DlgMessage *msg)
{

  if(!msg->SetIdentity(m_name))
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::PublishMessage(): Couldn't set identity for '%s' \n", m_name.c_str());
      return false;
    }
  
  if (!msg->SetServiceName(m_service))
    {
      Print(DBG_LEVEL_ERROR, "DlgPublisher::PublishMessage(): Couldn't set service name for '%s' \n", m_name.c_str());
      return false;
    }

  
  if (!msg->Send(m_socket))
    {
       Print(DBG_LEVEL_ERROR, "DlgPublisher::PublishMessage(): Couldn't send message \n");
      return false;
    }
  
  return true;
}
