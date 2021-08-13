# Gstreamer_StreamingVoiceToMultipleIPs

Ever wanted to stream voice to multiple destinations?

So, in this simple awesome project, I tried to stream voice into multiple adresses.

In Gstreamer you can stream voice, video, etc. to multiple destinations, I tried to add clients in this Gstreamer pipeline caled senderpipe, while it is on Playing state.

This project has a simple UI, a powerfull handled Gstreamer pipeline and a client manager to keep clients data.

all you need to do is to add a ip:port to create a new client and start streaming for that client, you can add many clients and remove any of them while it is on playing state, adding or removing client won't affect on other clients.

Have fun with Gstreamer...
