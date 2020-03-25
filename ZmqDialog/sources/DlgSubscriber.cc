#include "DlgSubscriber.h"

////**********************************************************////
////                 DlgSubscriber class                      ////
////**********************************************************////
namespace ZmqDialog {

DlgSubscriber::DlgSubscriber(const std::string &name) : m_name(name), m_service(""), m_server(""),
                            m_socket(nullptr), m_isRunning(false),
                            m_thread(nullptr)
{
  m_isRunning = true;
  m_thread = new std::thread(&DlgSubscriber::subscriber_thread, this);
}

DlgSubscriber::DlgSubscriber(const std::string &name,
                 const std::string &serviceName) : m_name(name), m_service(serviceName),
                                   m_server(""), m_socket(nullptr),
                                   m_isRunning(false), m_thread(nullptr)
{
  m_isRunning = true;
  m_thread = new std::thread(&DlgSubscriber::subscriber_thread, this);
}

DlgSubscriber::DlgSubscriber(const std::string &name,
                 const std::string &serviceName,
                 const std::string &serverName) : m_name(name), m_service(serviceName),
                                  m_server(serverName), m_socket(nullptr),
                                  m_isRunning(false), m_thread(nullptr)
{
  m_isRunning = true;
  m_thread = new std::thread(&DlgSubscriber::subscriber_thread, this);
}



DlgSubscriber::~DlgSubscriber()
{
  m_isRunning = false;
  if (m_thread && m_thread->joinable())
    m_thread->join();
  delete m_thread;

  //Deleting messages which weren't read
  while(!m_messages.empty())
    m_messages.pop();

  close_connection();
}

bool DlgSubscriber::SetServerName(const std::string &serverName)
{
    if (m_server != "")
    {
        Print(DBG_LEVEL_ERROR,
              "DlgSubscriber::SetServerName(): "
              "Subscriber %s already has active server (%s).\n"
              "If you want to change server, use ReConnect(server_name) instead.\n",
              m_name.c_str(), m_server.c_str());
        return false;
    }
    m_server = serverName;
    return true;
}

bool DlgSubscriber::SetServiceName(const std::string &serviceName)
{
    if (m_service != "")
    {
        Print(DBG_LEVEL_ERROR,
              "DlgSubscriber::SetServiceName(): "
              "Subscriber %s already has active service (%s).\n"
              "If you want to change service, use ReSubscribe(service_name) instead.\n",
              m_name.c_str(), m_service.c_str());
        return false;
    }
    m_service = serviceName;
    return true;
}

bool DlgSubscriber::Connect()
{
  if (IsConnected())
  {
      Print(DBG_LEVEL_ERROR,
            "DlgSubscriber::Connect(): "
            "Subscriber %s already has active connection with %s.\n"
            "If you want to change it, use ReConnect() instead.\n",
            m_name.c_str(), m_server.c_str());
      return false;
  }
  if (m_server == "")
    {
      Print(DBG_LEVEL_ERROR,
      "DlgSubscriber::Connect(): Subscriber %s has no server to connect.\n", m_name.c_str());
      return false;
    }
  //Connect to serve
  return connect_to(m_server.c_str());
}

bool DlgSubscriber::Connect(const std::string &serverName)
{
  m_server = serverName;
  return connect_to(m_server.c_str());
}

bool DlgSubscriber::Connect(const char *serverName)
{
    m_server = std::string(serverName);
    return connect_to(m_server.c_str());
}

bool DlgSubscriber::ReConnect(const std::string &serverName)
{
    if (IsConnected())
        close_connection();
    m_server = serverName;
    return Connect();
}

bool DlgSubscriber::Subscribe()
{
  if (!IsConnected())
    {
      Print(DBG_LEVEL_ERROR, "DlgSubscriber::Subscribe(): Subscriber %s is not connected at any socket.\n", m_name.c_str());
      return false;
    }

  if (m_service == "")
    {
      Print(DBG_LEVEL_ERROR, "DlgSubscriber::Subscribe(): Subscriber %s has no any services to subscribe.\n", m_name.c_str());
      return false;
    }

  DlgMessage *msg = new DlgMessage(m_service, m_name, m_server, SUBSCRIBE_TO_SERVICE, std::string(""));
  msg->SetIdentity(m_name);
  if (!msg->Send(m_socket))
    {
      delete msg;
      Print(DBG_LEVEL_ERROR,
            "DlgSubscriber::Subscribe(): Subscriber %s couldn't send "
            "a request to subscribe at service %s.\n",
            m_name.c_str(), m_service.c_str());
      return false;
    }
  delete msg;
  return true;
}

bool DlgSubscriber::Subscribe(const std::string &serviceName)
{
  m_service = serviceName;
  return Subscribe();
}

bool DlgSubscriber::Subscribe(const char* serviceName)
{
    m_service = std::string(serviceName);
    return Subscribe();
}

bool DlgSubscriber::ReSubscribe(const std::string &serviceName)
{
  if (IsConnected())
    close_connection();

  m_service = serviceName;

  if (!Connect(server_address))
  {
      Print(DBG_LEVEL_ERROR,
            "DlgSubscriber::ReSubscribe(): "
            "Couldn't connect to new server %s.\n", server_address);
      return false;
  }

  return Subscribe();
}

bool DlgSubscriber::ExtractMessage(DlgMessage *& msg)
{
  if (m_messages.empty())
    {
     Print(DBG_LEVEL_ERROR, "DlgSubscriber::GetMessage(): there are no any messages in queue.\n");
     return false;
    }

  m_mutex.lock();
  msg = m_messages.front();
  m_messages.pop();
  m_mutex.unlock();
  return true;
}

void DlgSubscriber::subscriber_thread()
{
  Print(DBG_LEVEL_DEBUG, "Start of %s subscriber thread.\n", m_name.c_str());
  while(m_isRunning)
  {
      if (!IsConnected())
        continue;

      zmq::pollitem_t items[] = {
        { static_cast<void*>(*m_socket), 0, ZMQ_POLLIN, 0 }
      };

      zmq::poll(items, 1, (long)TIMEOUT_INTERVAL/1000);

      if (items[0].revents & ZMQ_POLLIN)
    {
      DlgMessage *msg = new DlgMessage();
      if (!msg->Recv(m_socket))
        {
          Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscriber_thread(): message receiving error.\n");
          delete msg;
          continue;
        }

      uint32_t msgType = 0;
      if (!msg->GetMessageType(msgType))
        {
          Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscriber_thread(): bad message received(cannot get message type).\n");
          delete msg;
          continue;
        }
      //reply from server
      if (msgType == SUBSCRIBE_TO_SERVICE && !subscribe_to_service(msg))
        {
          Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscriber_thread(): Couldn't add a new service.\n");
          delete msg;
          continue;
        }
      //PUBLISH_TEXT_MESSAGE
      if (msgType == PUBLISH_TEXT_MESSAGE && !publish_text_message(msg))
        {
          Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscriber_thread(): Couldn't publish text message.\n");
          delete msg;
          continue;
        }

      //PUBLISH_BINARY_MESSAGE
      if (msgType == PUBLISH_BINARY_MESSAGE && !publish_binary_message(msg))
        {
          Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscriber_thread(): Couldn't publish binary message.\n");
          delete msg;
          continue;
        }
    }
  }//End of m_isRunning cycle
  Print(DBG_LEVEL_DEBUG, "End of %s subscriber's thread.\n", m_name.c_str());
}

void DlgSubscriber::close_connection()
{
  if (m_socket)
    m_socket->close();
  delete m_socket;
  m_socket = nullptr;
}

bool DlgSubscriber::connect_to(const char *name)
{
  if (IsConnected())
    close_connection();

  m_socket = ZMQ::Instance()->CreateSocket(ZMQ_DEALER);
  m_socket->setsockopt(ZMQ_IDENTITY, m_name.c_str(), m_name.size()+1);
  char endpoint[256];
  try
    {
      sprintf(endpoint, "tcp://%s", name);
      m_socket->connect(endpoint);
    }
  catch(zmq::error_t& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgSubscriber::connect_to(): zmq::exception %s\n", e.what());
      throw Exception("DlgSubscriber::connect_to(): fatal error.");
    }
  catch(std::exception& e)
    {
      Print(DBG_LEVEL_ERROR, "DlgSubscriber::connect_to(): std::exception %s\n", e.what());
      throw Exception("DlgSubscriber::connect_to(): fatal error.");
    }
  catch(...)
    {
      Print(DBG_LEVEL_ERROR, "DlgSubscriber::connect_to(): unknown exeption.\n");
      throw Exception("DlgSubscriber::connect_to(): fatal error.");
    }
  return true;
}

bool DlgSubscriber::subscribe_to_service(DlgMessage *msg)
{
  Print(DBG_LEVEL_DEBUG, "Subscribe to service message was received.\n");
  std::string brokerPort;
  if (!msg->GetMessageBody(brokerPort))
    {
      Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscribe_to_service(): "
                            "Couldn't get broker port for %s service.\n",
            m_service.c_str());
      return false;
    }

  Print(DBG_LEVEL_DEBUG,"DlgSubscriber::subscribe_to_service(): "
                        "Get broker port : '%s'.\n",
        brokerPort.c_str());

  if (!connect_to(brokerPort.c_str()))
    {
      Print(DBG_LEVEL_ERROR,"DlgSubscriber::subscribe_to_service(): Couldn't connect to broker %s.\n", brokerPort.c_str());
      return false;
    }
  delete msg;
  return true;
}

bool DlgSubscriber::publish_text_message(DlgMessage *msg)
{
  m_mutex.lock();
  m_messages.push(msg);
  m_mutex.unlock();

  std::string msgBody;
  if(msg->GetMessageBody(msgBody))
    {
      Print(DBG_LEVEL_DEBUG,"New text message <<%s>>\n", msgBody.c_str());
    }
  else
    {
      Print(DBG_LEVEL_ERROR,"DlgSubscriber::publish_text_message(): bad message body received.\n");
      return false;
    }
  return true;
}

bool DlgSubscriber::publish_binary_message(DlgMessage *msg)
{
  m_mutex.lock();
  m_messages.push(msg);
  m_mutex.unlock();

  void *buf = nullptr;
  size_t size = 0;
  if(msg->GetMessageBuffer(buf, size))
    {
      Print(DBG_LEVEL_DEBUG,"New binary message with size %ld\n", size);
    }
  else
    {
      Print(DBG_LEVEL_ERROR,"DlgSubscriber::publish_binary_message(): bad binary message received.\n");
      return false;
    }
  return true;
}

}//end of namespace
