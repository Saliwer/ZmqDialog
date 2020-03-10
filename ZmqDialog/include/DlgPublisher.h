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

using namespace ZmqDialog;
#define TIMEOUT_INTERVAL 2500000

////**********************************************************////
////                  DlgPublisher class                      ////
////**********************************************************////

class DlgPublisher
{
  std::string      m_name;
  std::string      m_service;
  zmq::socket_t*   m_socket;
  
public:
  DlgPublisher(const std::string &name, const std::string &service);
  virtual ~DlgPublisher();
  
  bool Register();
  bool PublishMessage(DlgMessage *msg);

};
#endif
