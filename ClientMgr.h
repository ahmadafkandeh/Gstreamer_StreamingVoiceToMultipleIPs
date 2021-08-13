#ifndef __CLIENTMGR_H__
#define __CLIENTMGR_H__

#include <gst/gst.h>

#include <string>
#include <list>


#define SUCCESS 0
#define CLIENT_ALREADY_ADDED 1
#define NO_CLIENT_IN_LIST 2
#define FAILURE -1

class ClientStruct
{
public:
    std::string ip;
    int port;
    GstElement * queue;
    GstElement * rtpEncoder;
    GstElement * rtpPayload;
    GstElement * queue2;
    GstElement * udpsink;
    GstPad     * tee_audio_pad;

public:
    explicit ClientStruct();
    void releaseElements();

    bool operator==(ClientStruct & that)
    {
        return this->ip == that.ip &&
                this->port == that.port;
    }

    ClientStruct& operator= (ClientStruct &ref)
    {
        this->ip              = ref.ip;
        this->port            = ref.port;
        this->queue           = ref.queue;
        this->rtpEncoder      = ref.rtpEncoder;
        this->rtpPayload      = ref.rtpPayload;
        this->queue2          = ref.queue2;
        this->udpsink         = ref.udpsink;
        this->tee_audio_pad   = ref.tee_audio_pad;
        return *this;
    }

};

class clientMgr
{
public:
    static clientMgr *instance();
    int add(std::string ip, int port);
    int remove(ClientStruct * client);
    int getClients(std::list<ClientStruct> &out);
    int getClient(std::string ip, int port, ClientStruct **out);
    int count()
    {
        return mClients.size();
    }

private:
    clientMgr();
    ~clientMgr();

private:
    std::list<ClientStruct> mClients;
};

#endif // __CLIENTMGR_H__
