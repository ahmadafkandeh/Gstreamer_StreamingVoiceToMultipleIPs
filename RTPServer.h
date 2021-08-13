#ifndef GST_RTP_SERVER_H
#define GST_RTP_SERVER_H

#include <gst/gst.h>

#include <thread>
#include <mutex>
#include <string>
#include <list>
#include <atomic>

#include "ClientMgr.h"



enum RTP_SERVER_ERR
{
    RTP_SERVER_SUCCESS =0,
    RTP_SERVER_ERR_FAILED =1,
    RTP_SERVER_ERR_CLIENT_EXIST,
    RTP_SERVER_ERR_INIT_FAILED,
    RTP_SERVER_ERR_ADD_FAILED
};

class RTPServer
{
public:
    RTPServer();
    ~RTPServer();
    RTP_SERVER_ERR AddClient(std::string ip, int remotePort);
    RTP_SERVER_ERR RemoveClient(std::string ip, int port);
    bool getClients(std::list<ClientStruct> &list);

private:

    void releaseElements();
    void start();
    void stop(bool dispose=false);
    void deInit();
    void ChangeElementState(GstElement * element, GstState state);
    void MainThreadHandler();
    void SetSelectorActivePad(GstElement * selector, GstPad * dstpad);
    void GetActivePadName(GstElement * selector);
    void Log(std::string data);
    int  addClientToPipeLine(ClientStruct *client);
    void removeClientFromPipeline(ClientStruct *cl);
    RTP_SERVER_ERR  Init();
    static int      UdpTx_bus_callback(GstBus *bus, GstMessage *message, gpointer data);

private:
    clientMgr  *mClients;
    GstBus     *bus;
    GstElement *Senderpipeline;
    GstElement *Audio_src;
    GstElement *Audio_convert;
    GstElement *Audio_resample;
    GstElement *Audio_queue;
    GstElement *Sender_tee;
    GstElement *SenderFakesink;
    GstElement *SenderFakesink_Queue;
    guint       sender_bus_watch_id;
    GMainLoop  *loop;

    bool        disposed=false;
    std::atomic<bool> mLibStarted;
    std::recursive_mutex _mutex;
    std::thread mainthread;
};

#endif // GST_RTP_SERVER_H
