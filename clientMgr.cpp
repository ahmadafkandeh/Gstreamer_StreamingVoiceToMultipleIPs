#include "ClientMgr.h"

#include <mutex>

ClientStruct::ClientStruct()
{
    ip              = "";
    port            = 0;
    queue           = nullptr;
    rtpEncoder      = nullptr;
    rtpPayload      = nullptr;
    queue2          = nullptr;
    udpsink         = nullptr;
    tee_audio_pad   = nullptr;
}
void ClientStruct::releaseElements()
{
    if (queue)
    {
        gst_object_unref (GST_OBJECT (queue));
    }

    if (rtpEncoder)
    {
        gst_object_unref (GST_OBJECT (rtpEncoder));
    }

    if (rtpPayload)
    {
        gst_object_unref (GST_OBJECT (rtpPayload));
    }

    if (queue2)
    {
        gst_object_unref (GST_OBJECT (queue2));
    }

    if (udpsink)
    {
        gst_object_unref (GST_OBJECT (udpsink));
    }

    if(tee_audio_pad)
    {
        gst_object_unref(GST_OBJECT (tee_audio_pad));
    }

    queue           = nullptr;
    rtpEncoder      = nullptr;
    rtpPayload      = nullptr;
    queue2          = nullptr;
    udpsink         = nullptr;
    tee_audio_pad   = nullptr;
}

clientMgr::clientMgr()
{
    mClients.clear();
}

clientMgr::~clientMgr()
{
    mClients.clear();
}

clientMgr* clientMgr::instance()
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lk(mtx);
    static clientMgr mInstance;

    return &mInstance;
}

int clientMgr::add(std::string ip, int port)
{
    for (auto it = mClients.begin() ; it != mClients.end() ; it++)
    {
        if(it->ip == ip && it->port == port)
        {
            return CLIENT_ALREADY_ADDED;
        }
    }
    
    ClientStruct client;
    client.ip = ip;
    client.port = port;
    mClients.push_back(client);

    return SUCCESS;
}

int clientMgr::remove(ClientStruct * client)
{
    for (auto it = mClients.begin() ; it != mClients.end() ; it++)
    {
        if(*it == *client)
        {
            mClients.erase(it++);
            return SUCCESS;
        }
    }

    return NO_CLIENT_IN_LIST;
}

int clientMgr::getClients(std::list<ClientStruct> & out)
{
    out.clear();
    if (mClients.size() == 0)
    {
        return NO_CLIENT_IN_LIST;
    }

    for (ClientStruct & cl : mClients)
    {
        out.push_back(cl);
    }
    return  SUCCESS;
}

int clientMgr::getClient(std::string ip, int port, ClientStruct **out)
{
    if (mClients.size() == 0)
    {
        return NO_CLIENT_IN_LIST;
    }

    for (ClientStruct & cl : mClients)
    {
        if (cl.ip == ip && cl.port == port)
        {
            *out = &cl;
            return  SUCCESS;
        }
    }
    return NO_CLIENT_IN_LIST;
}
