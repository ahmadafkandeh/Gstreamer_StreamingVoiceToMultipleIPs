#include "RTPServer.h"

#include <iostream>

static int Is_gst_init= false;

RTPServer::RTPServer()
{
    mLibStarted             = false;
    disposed                = true;
    mClients                = clientMgr::instance();
    Senderpipeline       = nullptr;
    Audio_src      = nullptr;
    Audio_convert  = nullptr;
    Audio_resample = nullptr;
    Audio_queue    = nullptr;
    Sender_tee           = nullptr;
    SenderFakesink       = nullptr;
    SenderFakesink_Queue = nullptr;
    Init();
}

RTPServer::~RTPServer()
{
    if(disposed)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lk (_mutex);
    stop(true);
    disposed=true;
}

void RTPServer::start()
{
    if (this->mLibStarted==true)
    {
        return;
    }

    mainthread = std::thread(&RTPServer::MainThreadHandler,this);

    Log("Waiting for Pipelines to start");
    while (mLibStarted.load() == false);
}


void RTPServer::deInit()
{
    ChangeElementState(SenderFakesink,GST_STATE_NULL);
    ChangeElementState(SenderFakesink_Queue,GST_STATE_NULL);
    ChangeElementState(Sender_tee,GST_STATE_NULL);
    ChangeElementState (Audio_queue, GST_STATE_NULL);
    ChangeElementState (Audio_resample, GST_STATE_NULL);
    ChangeElementState (Audio_convert, GST_STATE_NULL);
    ChangeElementState (Audio_src, GST_STATE_NULL);

    gst_bin_remove_many(GST_BIN(Senderpipeline),
                        Audio_src,
                        Audio_convert,
                        Audio_resample,
                        Audio_queue,
                        Sender_tee,
                        SenderFakesink,
                        SenderFakesink_Queue,
                        NULL);

    releaseElements();
}

void RTPServer::stop(bool dispose)
{
    std::list<ClientStruct> clients;
    std::lock_guard<std::recursive_mutex> lk (_mutex);

    if(this->mLibStarted == false)
    {
        if(dispose)
        {
            deInit();
        }
        return;
    }

    if (mClients->getClients(clients) == SUCCESS)
    {
        for (auto & cl: clients)
        {
            removeClientFromPipeline(&cl);
            cl.releaseElements();
            mClients->remove(&cl);
        }
    }

    if(dispose)
    {
        ChangeElementState (Senderpipeline, GST_STATE_NULL);
        Log("pipeline stopped");
    }

    g_main_loop_quit (loop);

    if(dispose)
    {
        deInit();
    }

    //waiting for Main loop to terminated
    while(this->mLibStarted ==true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if(mainthread.joinable())
    {
        mainthread.join();
    }
}

RTP_SERVER_ERR RTPServer::Init()
{
    if(!Is_gst_init)
    {
        /* Initialize GStreamer */
        gst_init (nullptr, nullptr);
        Is_gst_init = true;
    }
    Senderpipeline = gst_pipeline_new("udpSender_pipeline");
    if(!Senderpipeline)
    {
        g_print("failed to create pipeline\n");
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    Sender_tee = gst_element_factory_make("tee",nullptr);
    if(!Sender_tee)
    {
        g_print("failed to create element of type tee\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    //adding fakesink to Senderpipeline to prevent "data flow error" while there is only one element in pipeline.
    SenderFakesink = gst_element_factory_make("fakesink",nullptr);
    if(!SenderFakesink)
    {
        g_print("failed to create element of type fakesink\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }
    SenderFakesink_Queue = gst_element_factory_make("queue",nullptr);
    if(!SenderFakesink_Queue)
    {
        g_print("failed to create element of type queue\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }
    GstCaps *caps;
    Audio_src = gst_element_factory_make ("alsasrc", nullptr);
    if (!Audio_src)
    {
        Log ("Failed to create element of type Audio_src\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    //        g_object_set(Audio_src,"device",this->AudioDevice.c_str(),NULL);

    Audio_convert = gst_element_factory_make ("audioconvert",nullptr);
    if(!Audio_convert)
    {
        g_print("Failed to create element of type audioconvert\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }
    Audio_resample = gst_element_factory_make ("audioresample",nullptr);
    if(!Audio_resample)
    {
        g_print("Failed to create element of type Audio_resample\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    Audio_queue = gst_element_factory_make ("queue",nullptr);
    if(!Audio_queue)
    {
        g_print("Failed to create element of type Audio_queue\n");
        releaseElements();
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    gst_bin_add_many(GST_BIN(Senderpipeline),
                     Audio_src,
                     Audio_convert,
                     Audio_resample,
                     Audio_queue,
                     Sender_tee,
                     SenderFakesink_Queue,
                     SenderFakesink,
                     NULL);

    if (!gst_element_link_many (
                Audio_convert,
                Audio_resample,
                Audio_queue,
                Sender_tee,
                SenderFakesink_Queue,
                SenderFakesink,
                NULL))
    {
        g_warning ("Failed to link elements!");
        deInit();
        return RTP_SERVER_ERR_INIT_FAILED;
    }

    g_object_set(Audio_src,"provide-clock", FALSE,NULL);

    caps = gst_caps_new_simple ("audio/x-raw",
                                "endianness", G_TYPE_INT, 1234,
                                "signed", G_TYPE_BOOLEAN, TRUE,
                                "width", G_TYPE_INT, 16,
                                "depth", G_TYPE_INT, 16,
                                "channels", G_TYPE_INT, 1,
                                "rate", G_TYPE_INT, 8000,
                                NULL);
    gst_element_link_filtered(Audio_src,Audio_convert,caps);
    gst_caps_unref(caps);

    disposed = false;
    return RTP_SERVER_SUCCESS;
}

void RTPServer::releaseElements()
{
    if (Audio_src)
    {
        gst_object_unref(Audio_src);
    }

    if (Audio_convert)
    {
        gst_object_unref(Audio_convert);
    }

    if (Audio_resample)
    {
        gst_object_unref(Audio_resample);
    }

    if (Audio_queue)
    {
        gst_object_unref(Audio_queue);
    }

    if (Sender_tee)
    {
        gst_object_unref(Sender_tee);
    }

    if(SenderFakesink)
    {
        gst_object_unref(SenderFakesink);
    }

    if(SenderFakesink_Queue)
    {
        gst_object_unref(SenderFakesink_Queue);
    }

    if (Senderpipeline)
    {
        gst_object_unref (Senderpipeline);
    }

    Senderpipeline       = nullptr;
    Audio_src      = nullptr;
    Audio_convert  = nullptr;
    Audio_resample = nullptr;
    Audio_queue    = nullptr;
    Sender_tee           = nullptr;
    SenderFakesink       = nullptr;
    SenderFakesink_Queue = nullptr;
}

RTP_SERVER_ERR RTPServer::AddClient(std::string ip, int remotePort)
{
    std::lock_guard <std::recursive_mutex> lk (_mutex);
    if (mClients->add(ip, remotePort)== SUCCESS)
    {
        ClientStruct *cl;
        if (mClients->getClient(ip, remotePort, &cl) != SUCCESS)
        {
            Log("no client in list...");
            return RTP_SERVER_ERR_FAILED;
        }

        if (addClientToPipeLine(cl) == SUCCESS)
        {
            if(mLibStarted && Senderpipeline->current_state!=GST_STATE_PLAYING)
            {
                ChangeElementState (Senderpipeline, GST_STATE_PLAYING);
            }

            if(mLibStarted == false)
            {
                start();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            return RTP_SERVER_SUCCESS;
        }
        mClients->remove(cl);
        return RTP_SERVER_ERR_ADD_FAILED;
    }
    return RTP_SERVER_ERR_CLIENT_EXIST;

}


RTP_SERVER_ERR RTPServer::RemoveClient(std::string ip, int port)
{   
    std::lock_guard <std::recursive_mutex> lk (_mutex);
    ClientStruct *client;

    if(this->mClients->getClient(ip, port,&client) != SUCCESS)
    {
        return RTP_SERVER_ERR_FAILED;
    }

    removeClientFromPipeline(client);
    this->mClients->remove(client);

    if(this->mClients->count()==0)
    {
        this->stop();
    }

    return RTP_SERVER_SUCCESS;
}

void RTPServer::removeClientFromPipeline(ClientStruct *cl)
{
    if(cl->queue == nullptr )
    {
        return;
    }

    gst_element_release_request_pad (Sender_tee, cl->tee_audio_pad);

    gst_element_get_state (Sender_tee, nullptr, nullptr, -1);

    ChangeElementState (cl->udpsink, GST_STATE_NULL);
    ChangeElementState (cl->queue2, GST_STATE_NULL);
    ChangeElementState(cl->rtpPayload,GST_STATE_NULL);
    ChangeElementState(cl->rtpEncoder,GST_STATE_NULL);
    ChangeElementState (cl->queue, GST_STATE_NULL);

    gst_bin_remove_many(GST_BIN (Senderpipeline),
                        cl->queue,
                        cl->rtpEncoder,
                        cl->rtpPayload,
                        cl->queue2,
                        cl->udpsink,
                        NULL);

    cl->releaseElements();
}

int RTPServer::addClientToPipeLine(ClientStruct *client)
{
    GstPad *queue_audio_pad;
    client->queue = gst_element_factory_make("queue",nullptr);
    if(!client->queue)
    {
        g_print("failed to create element of type ClientSenderQueue\n");
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    client->queue2 = gst_element_factory_make("queue",nullptr);
    if(!client->queue2)
    {
        g_print("failed to create element of type ClientSenderQueue2\n");
        client->releaseElements();
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    client->rtpEncoder = gst_element_factory_make ("mulawenc",nullptr);
    if(!client->rtpEncoder)
    {
        client->releaseElements();
        g_print("Failed to create element of type mulawenc\n");
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    client->rtpPayload = gst_element_factory_make("rtppcmupay",nullptr);
    if(!client->rtpPayload)
    {
        client->releaseElements();
        g_print("Failed to create element of type rtppcmupay\n");
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    client->udpsink = gst_element_factory_make("udpsink",nullptr);
    if(!client->udpsink)
    {
        client->releaseElements();
        g_print("failed to create element of type ClientUdpSink\n");
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    g_object_set(G_OBJECT(client->udpsink), "port", client->port,NULL);
    g_object_set(G_OBJECT(client->udpsink), "host", client->ip.data(),NULL);
    g_object_set(G_OBJECT(client->rtpPayload),"buffer-list",true,NULL);

    gst_bin_add_many(GST_BIN(Senderpipeline),
                     client->queue,
                     client->rtpEncoder,
                     client->rtpPayload,
                     client->queue2,
                     client->udpsink,
                     NULL);

    gst_element_sync_state_with_parent (client->udpsink);
    gst_element_sync_state_with_parent (client->queue2);
    gst_element_sync_state_with_parent (client->queue);
    gst_element_sync_state_with_parent (client->rtpEncoder);
    gst_element_sync_state_with_parent (client->rtpPayload);

    if (!gst_element_link_many (client->queue,
                                client->rtpEncoder,
                                client->rtpPayload,
                                client->queue2,
                                client->udpsink,
                                NULL))
    {
        g_warning ("Failed to link elements!");
        gst_bin_remove_many(GST_BIN(Senderpipeline),
                            client->queue,
                            client->rtpEncoder,
                            client->rtpPayload,
                            client->queue2,
                            client->udpsink,
                            NULL);
        client->releaseElements();
        return RTP_SERVER_ERR_ADD_FAILED;
    }

    /* Manually link the Tee, which has "Request" pads */
    client->tee_audio_pad = gst_element_get_request_pad (Sender_tee, "src_%u");
    queue_audio_pad = gst_element_get_static_pad (client->queue, "sink");

    if (gst_pad_link (client->tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK)
    {
        Log ("Tee could not be linked\n");
    }

    gst_object_unref (queue_audio_pad);
    ChangeElementState(Senderpipeline,GST_STATE_PLAYING);
    return RTP_SERVER_SUCCESS;
}

bool RTPServer::getClients(std::list<ClientStruct> & list)
{
    std::lock_guard<std::recursive_mutex> lk (_mutex);
    return mClients->getClients(list) == SUCCESS;
}

int RTPServer::UdpTx_bus_callback (GstBus *bus, GstMessage *message, gpointer data)
{
    RTPServer* owner=(RTPServer*)data;
    GError *err;
    gchar *debug;
    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {

        gst_message_parse_error (message, &err, &debug);
        owner->Log ("Error: " + std::string(err->message) + std::to_string(err->code));
        g_error_free (err);
        g_free (debug);
        break;
    }
    case GST_MESSAGE_EOS:
        /* end-of-stream */
        //g_main_loop_quit (owner->loop);
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message,&err,&debug);
        owner->Log ("warning: " + std::string(err->message) + std::to_string(err->code));
        switch (err->code)
        {
        case 13: /* can not  record audio fast enough*/
            owner->ChangeElementState (owner->Senderpipeline, GST_STATE_PAUSED);
            owner->ChangeElementState (owner->Senderpipeline, GST_STATE_PLAYING);
            break;
        }
        g_error_free (err);
        g_free (debug);
        break;
    case GST_MESSAGE_CLOCK_LOST:
        owner->Log( "Got message: " + std::string(GST_MESSAGE_TYPE_NAME (message)));
        owner->ChangeElementState (owner->Senderpipeline, GST_STATE_PAUSED);
        owner->ChangeElementState (owner->Senderpipeline, GST_STATE_PLAYING);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        break;
    case GST_MESSAGE_STREAM_STATUS:
        break;
    case GST_MESSAGE_LATENCY:
        break;
    default:
        /* unhandled message */
        owner->Log( "Got message: " + std::string(GST_MESSAGE_TYPE_NAME (message)));
        break;
    }
    /* we want to be notified again the next time there is a message
    * on the bus, so returning TRUE (FALSE means we want to stop watching
    * for messages on the bus and our callback should not be called again)
    */
    return TRUE;
}

void RTPServer::MainThreadHandler()
{
    /* adds a watch for new message on our pipeline’s message bus to
    * the default GLib main context, which is the main context that our
    * GLib main loop is attached to below
    */
    bus = gst_pipeline_get_bus (GST_PIPELINE (Senderpipeline));
    sender_bus_watch_id = gst_bus_add_watch (bus, RTPServer::UdpTx_bus_callback, this);
    gst_object_unref (bus);

    /* Start playing */
    ChangeElementState (Senderpipeline, GST_STATE_PLAYING);

    /* create a mainloop that runs/iterates the default GLib main context
  * (context NULL), in other words: makes the context check if anything
  * it watches for has happened. When a message has been posted on the
  * bus, the default main context will automatically call our
  * my_bus_callback() function to notify us of that message.
  * The main loop will be run until someone calls g_main_loop_quit()
  */
    loop = g_main_loop_new (nullptr, FALSE);
    this -> mLibStarted = true;
    g_main_loop_run (loop);
    Log("MAIN LOOP STOPPED");

    ChangeElementState (Senderpipeline, GST_STATE_NULL);

    Log("Senderpipeline STOPPED");

    g_source_remove (sender_bus_watch_id);

    g_main_loop_unref (loop);

    this->mLibStarted = false;
    Log("MAIN LOOP Terminated");
}

void RTPServer::Log(std::string data)
{
    std::cout << data << std::endl;
}

void RTPServer::ChangeElementState(GstElement * element, GstState state)
{
    GstStateChangeReturn ret;

    if(element->next_state != GST_STATE_VOID_PENDING)
    {
        if(element->current_state == GST_STATE_READY)
        {
            gst_element_set_state (element, GST_STATE_NULL);
        }

        if(element->current_state == GST_STATE_PAUSED)
        {
            gst_element_set_state (element, GST_STATE_READY);
        }
    }

    ret = gst_element_set_state (element, state);
    switch ((int)ret)
    {
    case GST_STATE_CHANGE_SUCCESS:
        Log ("GST_STATE_CHANGE_SUCCESS");
        break;
    case GST_STATE_CHANGE_FAILURE:
        Log ("GST_STATE_CHANGE_FAILURE");
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        Log ("GST_STATE_CHANGE_NO_PREROLL");
        break;
    case GST_STATE_CHANGE_ASYNC:
        Log ("GST_STATE_CHANGE_ASYNC");
        break;
    default:
        break;
    }
}
