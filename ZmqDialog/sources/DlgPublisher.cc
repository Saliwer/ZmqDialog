#include "DlgPublisher.h"

////**********************************************************////
////                  DlgPublisher class                      ////
////**********************************************************////
namespace ZmqDialog {

DlgPublisher::DlgPublisher(const std::string &name) : m_name(name), m_service(""),
    m_server(""), m_socket(nullptr)
{
    m_isRunning = true;
    m_thread = new std::thread(&DlgPublisher::publisher_thread, this);
}


DlgPublisher::DlgPublisher(const std::string &name, const std::string &service) :
    m_name(name), m_service(service), m_server(""), m_socket(nullptr)
{
    m_isRunning = true;
    m_thread = new std::thread(&DlgPublisher::publisher_thread, this);
}

DlgPublisher::DlgPublisher(const std::string &name, const std::string &service,
                           const std::string &serverName) : m_name(name),
    m_service(service), m_server(serverName), m_socket(nullptr)
{
    m_isRunning = true;
    m_thread = new std::thread(&DlgPublisher::publisher_thread, this);
}

DlgPublisher::~DlgPublisher()
{
    m_isRunning = false;
    if (m_thread->joinable())
        m_thread->join();
    delete m_thread;

    close_connection();
}

bool DlgPublisher::SetServerName(const std::string &serverName)
{
    if (m_server != "")
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::SetServerName(): "
              "Publisher %s already has active server (%s).\n"
              "If you want to change server, use ReConnect(server_name) instead.\n",
              m_name.c_str(), m_server.c_str());
        return false;
    }
    m_server = serverName;
    return true;
}

bool DlgPublisher::Connect()
{
    if (IsConnected())
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::Connect(): "
              "Publisher %s already has active connection with %s .\n"
              "If you want to change it, use ReConnect() instead.\n",
              m_name.c_str(), m_server.c_str());
        return false;
    }
    if (m_server == "")
    {
        Print(DBG_LEVEL_DEBUG,
              "DlgPublisher::Connect(): Publisher %s has no server to connect\n", m_name.c_str());
        return false;
    }
    return connect_to(m_server.c_str());
}

bool DlgPublisher::Connect(const std::string &serverName)
{
    m_server = serverName;
    return connect_to(m_server.c_str());
}

bool DlgPublisher::Connect(const char *serverName)
{
    m_server = std::string(serverName);
    return connect_to(m_server.c_str());
}

bool DlgPublisher::ReConnect(const std::string &serverName)
{
    if (IsConnected())
        close_connection();
    m_server = serverName;
    return Connect();
}

bool DlgPublisher::Register()
{
    if (!IsConnected())
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::Register(): Publisher %s has no any active connections\n",
              m_name.c_str());
        return false;
    }

    if (m_service == "")
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::Register(): Publisher %s has no any active services\n",
              m_name.c_str());
        return false;
    }

    DlgMessage *msg = new DlgMessage(m_service, m_name, m_server, REGISTER_PUBLISHER, std::string(""));
    if (!msg->SetIdentity(m_name))
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::Register(): Coldn't set identity for %s publisher\n",
              m_name.c_str());
        delete msg;
        return false;
    }

    if (!msg->Send(m_socket))
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::Register(): Publisher %s coldn't send "
              "a request to register at %s service\n",
              m_name.c_str(), m_service.c_str());
        delete msg;
        return false;
    }
    delete msg;
    return true;
}

bool DlgPublisher::ReRegister(const std::string &serviceName)
{
    if (IsConnected())
        close_connection();

    if (!Connect(server_address))
    {
        Print(DBG_LEVEL_ERROR,
              "DlgPublisher::ReRegister(): "
              "Couldn't connect to new server %s.\n", server_address);
        return false;
    }
    m_service = serviceName;
    return Register();
}

bool DlgPublisher::PublishMessage(DlgMessage *msg)
{
    if (!msg->SetServiceName(m_service))
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::PublishMessage(): Couldn't set service name for '%s' \n", m_name.c_str());
        return false;
    }
    return send_message(msg, m_name);
}

bool DlgPublisher::send_message(DlgMessage *msg, const std::string &identity)
{
    if(!msg->SetIdentity(identity))
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::send_message(): Couldn't set identity for '%s' \n", m_name.c_str());
        return false;
    }

    if (!msg->Send(m_socket))
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::PublishMessage(): Couldn't send message \n");
        return false;
    }

    return true;
}

bool DlgPublisher::connect_to(const char *serverName)
{
    if (IsConnected())
        close_connection();

    m_socket = ZMQ::Instance()->CreateSocket(ZMQ_DEALER);
    m_socket->setsockopt(ZMQ_IDENTITY, m_name.c_str(), m_name.size() + 1);

    char endpoint[256];
    try
    {
        sprintf(endpoint, "tcp://%s", serverName);
        m_socket->connect(endpoint);
    }
    catch(zmq::error_t& e)
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::connect_to() zmq::exception %s\n", e.what());
        throw Exception("DlgPublisher::connect_to() fatal error.");
    }
    catch(std::exception& e)
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::connect_to() std::exception %s\n", e.what());
        throw Exception("DlgPublisher::connect_to() fatal error.");
    }
    catch(...)
    {
        Print(DBG_LEVEL_ERROR, "DlgPublisher::connect_to() unknown exeption.\n");
        throw Exception("DlgPublisher::connect_to() fatal error.");
    }
    return true;
}

void DlgPublisher::close_connection()
{
    if (m_socket)
        m_socket->close();
    delete m_socket;
    m_socket = nullptr;
}

void DlgPublisher::publisher_thread()
{
    Print(DBG_LEVEL_DEBUG,
          "Start of %s publisher's thread\n",
          m_name.c_str());

    size_t liveness = HEARTBEAT_LIVENESS;

    while (m_isRunning)
    {
        if (!IsConnected())
            continue;

        zmq::pollitem_t items[] = {
            { static_cast<void*>(*m_socket), 0, ZMQ_POLLIN, 0 }
        };

        zmq::poll(items, 1, HEARTBEAT_INTERVAL/1000);

        if (items[0].revents & ZMQ_POLLIN)
        {
            liveness = HEARTBEAT_LIVENESS;
            DlgMessage *msg = new DlgMessage();
            if (!msg->Recv(m_socket))
            {
                Print(DBG_LEVEL_ERROR,"DlgPublisher::publisher_thread(): "
                                      "message receiving error.\n");
                delete msg;
                continue;
            }
            MessageType msgType;
            if (!msg->GetMessageType(msgType))
            {
                Print(DBG_LEVEL_ERROR,"DlgPublisher::publisher_thread(): "
                                      "bad message received(cannot get message type).\n");
                delete msg;
                continue;
            }
            //reply from server
           if (msgType == REGISTER_PUBLISHER && !register_publisher(msg))
            {
                Print(DBG_LEVEL_ERROR,"DlgPublisher::publisher_thread(): "
                                      "Couldn't register publisher.\n");
                delete msg;
                continue;
            }
            if (msgType == HEARTBEAT_PING)
            {
                Print(DBG_LEVEL_DEBUG,
                      "PUB(%s): Send HEARTBEAT_PONG message\n", m_name.c_str());
                DlgMessage *heartbeat_msg = new DlgMessage();
                heartbeat_msg->SetMessageType(HEARTBEAT_PONG);
                send_message(heartbeat_msg, m_name);
                delete heartbeat_msg;
                delete msg;
                continue;
            }

        }
        else
        {
            if (--liveness == 0)
            {
                Print(DBG_LEVEL_DEBUG,
                      "PUB(%s): heartbeating failure, can't reach queue\n", m_name.c_str());
                Print(DBG_LEVEL_DEBUG,
                      "PUB(%s): Trying to reconnect\n", m_name.c_str());
                if (!Register())
                {
                    Print(DBG_LEVEL_DEBUG,
                          "PUB(%s)% Couldn't reconnect to server '%s'\n",
                          m_name.c_str(), m_server.c_str());
                   liveness = HEARTBEAT_LIVENESS;
                   //m_isRunning = false;
                }
                else
                    liveness = HEARTBEAT_LIVENESS;
            }
        }

    }
    Print(DBG_LEVEL_DEBUG,
          "End of %s publisher's thread\n",
          m_name.c_str());
}

bool DlgPublisher::register_publisher(DlgMessage *msg)
{
    Print(DBG_LEVEL_DEBUG,
          "Publisher %s received register_publisher message\n",
          m_name.c_str());
    std::string brokerPort;
    if (!msg->GetMessageBody(brokerPort))
    {
        Print(DBG_LEVEL_ERROR,"DlgPublisher::register_publisher(): "
                              "Couldn't get broker port for %s service.\n",
              m_service.c_str());
        return false;
    }

    Print(DBG_LEVEL_DEBUG,"DlgPublisher::register_publisher(): "
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

}//end of namespace
