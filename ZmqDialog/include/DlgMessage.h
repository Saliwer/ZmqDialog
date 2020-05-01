#ifndef __DLG_MESSAGE_H__
#define __DLG_MESSAGE_H__

#include <stdint.h>

#include <zmq.hpp>

#include <vector>
#include <string>
#include <map>

namespace ZmqDialog {

enum MessageType
{
    EMPTY_MESSAGE = 0,
    PUBLISH_TEXT_MESSAGE,
    PUBLISH_BINARY_MESSAGE,
    SUBSCRIBE_TO_SERVICE,
    REGISTER_PUBLISHER,
    HEARTBEAT_PING,
    HEARTBEAT_PONG
};


class message_array_t
{
protected:
    typedef std::vector<uint8_t> byte_array_t;
    std::vector<byte_array_t> m_data;
    std::vector<uint8_t> m_identity;
public:
    message_array_t() {};
    message_array_t(const char* str);
    message_array_t(const message_array_t& msg);
    virtual ~message_array_t() {};

    void   Clear()           { m_data.clear(); }
    size_t GetNParts() const { return m_data.size(); }

    //Update:  if idx < 0 then new element is pushing back:
    bool Update(int idx, void* buf, size_t size);
    bool Update(int idx, const char* str);

    void PushFront(void* buf, size_t size);
    void PushFront(const char* str);

    void PushBack(void* buf, size_t size);
    void PushBack(const char* str);

    bool PopFront(std::string& str);
    bool PopFront(void* buf, size_t& size);

    bool Recv(zmq::socket_t* socket);
    bool Send(zmq::socket_t* socket);
};

class DlgMessage : protected message_array_t
{
    const size_t N_FIELDS = 5;
    MessageType _type;
public:
    DlgMessage();
    DlgMessage(const std::string& name, const std::string& from, const std::string& to,
               MessageType msgType, const std::string& body);
    virtual ~DlgMessage();


    bool GetServiceName(std::string& name);
    bool GetFromAddress(std::string& address);
    bool GetToAddress(std::string& address);
    bool GetMessageType(MessageType& msgType);
    bool GetMessageBody(std::string& body);
    bool GetMessageBuffer(void* buf, size_t& size);
    bool GetIdentity(std::string& identity);

    bool SetServiceName(const std::string& name);
    bool SetFromAddress(const std::string& address);
    bool SetToAddress(const std::string& address);
    bool SetMessageType(MessageType msgType);
    bool SetMessageBody(const std::string& body);
    bool SetMessageBuffer(void* buf, size_t size);
    bool SetIdentity(const std::string &identity);

    message_array_t* GetMessageArray()           { return (message_array_t*)this;          }
    bool             Recv(zmq::socket_t* socket) { return GetMessageArray()->Recv(socket); }
    bool             Send(zmq::socket_t* socket) { return GetMessageArray()->Send(socket); }
    void             PrintMessage(FILE* out);
};



}

#endif // __DLG_MESSAGE_H__
