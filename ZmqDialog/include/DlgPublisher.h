#ifndef DLG_PUBLISHER_H
#define DLG_PUBLISHER_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <zmq.hpp>
#include "DlgServer.h"
#include "Config.h"
#include "Debug.h"
#include "DlgMessage.h"
#include "Exception.h"


////**********************************************************////
////                  DlgPublisher class                      ////
////**********************************************************////
namespace ZmqDialog
{

class DlgPublisher
{
  std::string      m_name;
  std::string      m_service;
  std::string      m_server;
  zmq::socket_t*   m_socket;
  std::mutex       m_mutex;
  bool             m_isRunning;
  std::thread*     m_thread;


public:

  DlgPublisher(const std::string &name);
  DlgPublisher(const std::string &name, const std::string &service);
  DlgPublisher(const std::string &name, const std::string &service,
               const std::string &serverName);
  virtual ~DlgPublisher();

  bool SetServerName(const std::string &serverName);

  bool Connect();
  bool Connect(const std::string &serverName);
  bool Connect(const char* serverName);
  bool ReConnect(const std::string &serverName);

  bool PublishMessage(DlgMessage *msg);

  bool Register();
  bool ReRegister(const std::string &serviceName);

  bool IsConnected(){ return m_socket; }
private:
  bool connect_to(const char* serverName);
  void close_connection();
  void publisher_thread();

  bool send_message(DlgMessage *msg, const std::string &identity);
  //Parsing received messages
  bool register_publisher(DlgMessage *msg);
};

}//end of namespace ZmqDialog

#endif
