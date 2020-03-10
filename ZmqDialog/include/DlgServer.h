#ifndef __DLG_SERVER_H__
#define __DLG_SERVER_H__

#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

#include <zmq.hpp>

namespace ZmqDialog
{


  ////**********************************************************////
  ////                        ZMQ class                         ////
  ////**********************************************************////
  class ZMQ
  {
    static zmq::context_t* m_context;
    static ZMQ*            m_instance;
  private:
    ZMQ();
  public:
    static ZMQ* Instance();
    ~ZMQ();
    zmq::context_t* Context() { return m_context; }
    zmq::socket_t*  CreateSocket(int socket_type);
  };


  ////**********************************************************////
  ////                   aSubscriber class                      ////
  ////**********************************************************////

  class DlgMessage;

  class aSubscriber
  {
    std::string             m_id;
    int64_t                 m_expiry;    //  Expiries at unless heartbeat
  public:
  aSubscriber(const char* id, int64_t expiry = 0) : m_expiry(expiry)
    {
      m_id = id;
    }

    std::string GetID() const { return m_id; }
  };


  ////**********************************************************////
  ////                    aPublisher class                      ////
  ////**********************************************************////

  class aPublisher
  {
    std::string             m_id;
    int64_t                 m_expiry;    //  Expiries at unless heartbeat
  public:
  aPublisher(const char* id, int64_t expiry = 0) : m_expiry(expiry)
    {
      m_id = id;
    }
    std::string GetID() const { return m_id; }
  };

  ////**********************************************************////
  ////                   aService class                         ////
  ////**********************************************************////

  class aBroker;

  class aService
  {
    std::string                         m_name;
    aBroker*                            m_broker;
    std::mutex                          m_mutex;
  public:
    explicit aService(const char* name);
    virtual ~aService();

    bool ReleaseMessage(DlgMessage* msg);
    aBroker* GetBroker() { return m_broker; }
  
  };


  ////**********************************************************////
  ////                   aBroker  class                         ////
  ////**********************************************************////

  class aBroker
  {
    std::string                         m_name;
    std::thread*                        m_thread;
    std::mutex                          m_mutex;
    std::map<std::string, aSubscriber*> m_subscribers;
    std::map<std::string, aPublisher*>  m_publishers;
    std::vector<DlgMessage*>            m_requests;
    bool                                m_isRunning;
    zmq::socket_t*                      m_socket;
    std::string                         m_port;
  public:
    aBroker(const char* name);
    ~aBroker();
    bool AddRequest(DlgMessage* msg);
    bool AddSubscriber(const char *id);
    bool AddPublisher(const char *id);
    bool SendMessage(DlgMessage* msg, aSubscriber* s);
    std::string GetPort() const { return m_port; };
  private:
    void broker_thread();
    void delete_publisher(const char* id);
    void destroy_publishers();  
    
    bool publish_text_message(DlgMessage *msg);
    bool publish_binary_message(DlgMessage *msg);
    bool subscribe_to_service(DlgMessage *msg);
    bool register_publisher(DlgMessage *msg);
  };


  ////**********************************************************////
  ////                  DlgServer class                         ////
  ////**********************************************************////

class DlgServer
  {
    zmq::socket_t*  m_router;
    std::thread*    m_main_thread;

  protected:
    std::map<std::string, aService*>    m_services;
  public:
    DlgServer();
    ~DlgServer();
  
    bool Start();
    void Stop();

  private:
    void main_thread();

    bool create_service(const char* name);
    volatile static bool m_isRunning; 

    
    bool subscribe_to_service(DlgMessage *msg);
    bool register_publisher(DlgMessage *msg);
    
  };


} // namespace ZmqDialog

#endif
