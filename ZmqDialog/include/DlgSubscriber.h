#ifndef _DLG_SUBSCRIBER_H_
#define _DLG_SUBSCRIBER_H_

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <queue>

#include <zmq.hpp>
#include "DlgServer.h"
#include "Config.h"
#include "Debug.h"
#include "DlgMessage.h"
#include "Exception.h"

#include <ctime>



////**********************************************************////
////                 DlgSubscriber class                      ////
////**********************************************************////
namespace ZmqDialog
{

class DlgSubscriber
{
private:
  std::string             m_name;
  std::string             m_service;
  std::string             m_server;
  zmq::socket_t*          m_socket;
  std::mutex              m_mutex;
  bool                    m_isRunning;
  std::thread*            m_thread;

  std::queue<DlgMessage*> m_messages;

public:

  DlgSubscriber(const std::string &name);
  DlgSubscriber(const std::string &name, const std::string &serviceName);
  DlgSubscriber(const std::string &name, const std::string &serviceName,
        const std::string &serverName);

  virtual ~DlgSubscriber();

  bool SetServerName(const std::string &serverName);

  bool SetServiceName(const std::string &serviceName);

  bool Connect();
  bool Connect(const std::string &serverName);
  bool Connect(const char* serverName);
  bool ReConnect(const std::string &serverName);

  bool IsConnected() { return m_socket; }

  bool Subscribe();
  bool Subscribe(const std::string &serviceName);
  bool Subscribe(const char* serviceName);
  bool ReSubscribe(const std::string &serviceName);

  bool HasData() { return !m_messages.empty(); }

  bool ExtractMessage(DlgMessage *& msg);

private:

  void subscriber_thread();

  bool connect_to(const char* name);
  void close_connection();

  bool send_message(DlgMessage* msg, const std::string &identity);
  bool subscribe_to_service(DlgMessage *msg);
  bool publish_text_message(DlgMessage *msg);
  bool publish_binary_message(DlgMessage *msg);

};

}//end of namespace ZmqDialog


#endif
