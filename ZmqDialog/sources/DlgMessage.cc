#include <stdint.h>
#include <string.h>

#include <string>

#include <zmq.hpp>

#include "ZmqDialog.h"
#include "DlgMessage.h"
#include "Debug.h"

namespace ZmqDialog
{
  message_array_t::message_array_t(const char* str)
  {
    uint32_t str_size = strlen(str)+1;
    byte_array_t s(sizeof(str_size)+str_size);
    memcpy(s.data(),&str_size,sizeof(str_size));
    memcpy(s.data()+sizeof(str_size),str,str_size);
    m_data.push_back(s);
  }

  message_array_t::message_array_t(const message_array_t& msg)
  {
    m_data.resize(msg.m_data.size());
    std::copy(msg.m_data.begin(), msg.m_data.end(), m_data.begin());
  }

  bool message_array_t::Update(int idx, void* buf, size_t size)
  {
    if(idx >= (int)m_data.size())
      return false;
    uint32_t buf_size = (uint32_t)size;
    byte_array_t s(sizeof(buf_size)+size);
    memcpy(s.data(),&buf_size,sizeof(buf_size));
    memcpy(s.data()+sizeof(buf_size),buf,buf_size);
    if(idx >= 0 && idx < (int)m_data.size())
      m_data[idx] = s;
    else // negative index or next to last
      m_data.push_back(s);
    return true;
  }

  bool message_array_t::Update(int idx, const char* str)
  {
    if(idx > (int)m_data.size())
      return false;
    uint32_t str_size = strlen(str)+1;
    byte_array_t s(sizeof(str_size)+str_size);
    memcpy(s.data(),&str_size,sizeof(str_size));
    memcpy(s.data()+sizeof(str_size),str,str_size);
    if(idx >= 0 && idx < (int)m_data.size())
      m_data[idx] = s;
    else // negative index or next to last
      m_data.push_back(s);
    return true;
  }

  void message_array_t::PushFront(void* buf, size_t size)
  {
    uint32_t buf_size = (uint32_t)size;
    byte_array_t s(sizeof(buf_size)+size);
    memcpy(s.data(),&buf_size,sizeof(buf_size));
    memcpy(s.data()+sizeof(buf_size),buf,buf_size);
    m_data.insert(m_data.begin(),s);
  }

  void message_array_t::PushFront(const char* str)
  {
    uint32_t str_size = strlen(str)+1;
    byte_array_t s(sizeof(str_size)+str_size);
    memcpy(s.data(),&str_size,sizeof(str_size));
    memcpy(s.data()+sizeof(str_size),str,str_size);
    m_data.insert(m_data.begin(),s);
  }

  void message_array_t::PushBack(void* buf, size_t size)
  {
    Update(-1,buf,size);
  }

  void message_array_t::PushBack(const char* str)
  {
    Update(-1,str);
  }

  bool message_array_t::PopFront(std::string& str)
  {
    if(m_data.size() == 0)
      return false;
    byte_array_t s = m_data.front();
    m_data.erase(m_data.begin());
    if(s.size() < sizeof(uint32_t))
      return false;
    uint32_t size = *(uint32_t*)s.data();
    s.erase(s.begin(),s.begin()+sizeof(uint32_t));
    if(s.size() != size)
      return false;
    str = (char*)s.data();
    return true;
  }

  bool message_array_t::PopFront(void* buf, size_t& size)
  {
    if(m_data.size() == 0)
      return false;
    byte_array_t s = m_data.front();
    m_data.erase(m_data.begin());
    if(s.size() < sizeof(uint32_t))
      return false;
    uint32_t usize = *(uint32_t*)s.data();
    if((size_t)usize > size)
      {
	size = (size_t)usize;
	return false;
      }
    size = (size_t)usize;
    memcpy(buf,s.data()+sizeof(uint32_t),size);
    return true;
  }

  bool message_array_t::Recv(zmq::socket_t* socket)
  {
    if(!socket)
      return false;
    Clear();
    while(!IS_INTERRUPTED)
      {
	zmq::message_t message(0);
	try
	  {
	    if (!socket->recv(&message, 0))
	      {
		return false;
	      }
	  }
	catch(zmq::error_t error)
	  {
	    Print(DBG_LEVEL_ERROR,"Recv: message receiving error: %s\n", error.what());
	    return false;
	  }
	byte_array_t frame(message.size());
	memcpy(frame.data(),message.data(),message.size());
	m_data.push_back(frame);
	if(!message.more())
	  {
	    break;
	  }
      }
    return true;
  }
 
  bool message_array_t::Send(zmq::socket_t* socket)
  {
    if(!socket)
      return false;
    bool is_error = false;
    try
      {
	for(size_t i = 0; i < m_data.size(); i++)
	  {
	    zmq::message_t message(m_data[i].data(),m_data[i].size());
	    socket->send(message, i < m_data.size() - 1 ? ZMQ_SNDMORE : 0);
	  }
      }
    catch(zmq::error_t error)
      {
	Print(DBG_LEVEL_ERROR,"Send: message sending error %d (%s)\n", zmq_errno(), zmq_strerror(zmq_errno()));
	is_error = true;
      }
    return (is_error==false);
  }

  ////////////////////////// class DlgMessage ///////////////////////////

  DlgMessage::DlgMessage() : message_array_t()
  {
    PushBack(""); // service name
    PushBack(""); // from address
    PushBack(""); // to address
    uint32_t msgType = EMPTY_MESSAGE;
    PushBack(&msgType,sizeof(msgType));
    PushBack(""); // empty body
  }

  DlgMessage::DlgMessage(const std::string& name, const std::string& from, const std::string& to, 
			 uint32_t msgType, const std::string& body) : message_array_t()
  {
    PushBack(name.c_str());
    PushBack(from.c_str());
    PushBack(to.c_str());
    PushBack(&msgType,sizeof(msgType));
    PushBack(body.c_str());
  }

  DlgMessage::~DlgMessage()
  {
  }

  bool DlgMessage::GetServiceName(std::string& name)
  {
    const int idx = 0;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() < sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != m_data[idx].size()-sizeof(uint32_t))
      return false;
    if(s != 0)
      {
	name = (char*)m_data[idx].data()+sizeof(uint32_t);
	if(name.size()+1 != s)
	  return false;
      }
    else
      name = "";
    return true;
  }
  
  bool DlgMessage::GetFromAddress(std::string& address)
  {
    const int idx = 1;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() < sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != m_data[idx].size()-sizeof(uint32_t))
      return false;
    if(s != 0)
      {
	address = (char*)m_data[idx].data()+sizeof(uint32_t);
	if(address.size()+1 != s)
	  return false;
      }
    else
      address = "";
    return true;
  }

  bool DlgMessage::GetToAddress(std::string& address)
  {
    const int idx = 2;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() < sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != m_data[idx].size()-sizeof(uint32_t))
      return false;
    if(s != 0)
      {
	address = (char*)m_data[idx].data()+sizeof(uint32_t);
	if(address.size()+1 != s)
	  return false;
      }
    else
      address = "";
    return true;
  }

  bool DlgMessage::GetMessageType(uint32_t& msgType)
  {
    const int idx = 3;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() != 2*sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != sizeof(uint32_t))
      return false;
    msgType = *((uint32_t*)m_data[idx].data()+1);
    return true;
  }

  bool DlgMessage::GetMessageBody(std::string& body)
  {
    const int idx = 4;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() < sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != m_data[idx].size()-sizeof(uint32_t))
      return false;
    if(s != 0)
      {
	body = (char*)m_data[idx].data()+sizeof(uint32_t);
	if(body.size()+1 != s)
	  return false;
      }
    else
      body = "";
    return true;
  }

  bool DlgMessage::GetMessageBuffer(void* buf, size_t& size)
  {
    const int idx = 4;
    if(GetMessageArray()->GetNParts() < N_FIELDS || m_data[idx].size() < sizeof(uint32_t))
      return false;
    uint32_t s = *(uint32_t*)m_data[idx].data();
    if(s != m_data[idx].size()-sizeof(uint32_t))
      return false;
    if(size == 0)
      {
	size = s;
	return true;
      }
    if(s > size)
      {
	size = s;
	return false;
      }
    if(!buf)
      return false;
    if(s != 0)
      {
	memcpy(buf,(char*)m_data[idx].data()+sizeof(uint32_t),s);
      }
    size = s;
    return true;
  }


  bool DlgMessage::SetServiceName(const std::string& name)
  {
    return GetMessageArray()->Update(0,name.c_str());
  }

  bool DlgMessage::SetFromAddress(const std::string& address)
  {
    return GetMessageArray()->Update(1,address.c_str());
  }

  bool DlgMessage::SetToAddress(const std::string& address)
  {
    return GetMessageArray()->Update(2,address.c_str());
  }

  bool DlgMessage::SetMessageType(uint32_t msgType)
  {
    return GetMessageArray()->Update(3,&msgType,sizeof(msgType));  
  }

  bool DlgMessage::SetMessageBody(const std::string& body)
  {
    return GetMessageArray()->Update(4,body.c_str());
  }

  bool DlgMessage::SetMessageBuffer(void* buf, size_t size)
  {
    return GetMessageArray()->Update(4,buf,size);
  }

} // namespace ZmqDialog

