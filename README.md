ModBusServer
============

This is sample of ModBusServer part. Only for evaluation purposes, not for any other use - commercial or free.
Absolutely no warranty of work for any parts. 


ModBusServer is a platform for distributed messages processing. Based on ZeroMQ high-level API and MessagePack binary packing, it provide load balancing, timeouts processing, automated self-discovering and modules monitoring. Modules may be in any language, that have ZMQ library support. With Nginx module enabled, calls to Nginx in form of FCGI requests will be routed to MBS server by some rule depend of any FCGI params. Modules can call each other via MBS server by command protocol based on request-reply pattern, server guarantee the reply (with error, timeout or success). Messages stored in FIFO queue handled by server, if server didn't found module locally, it try to find module on other machines.

MBS have autodiscovery feature, that automatically let know each other server on new server start in network, so it easy to add/remove servers if needed. Every MBS server have a config file, that describe modules need to run as normal unix process. So modules run on server start, send register command and stay monitored by server. If module in stuck and timeout exceed, it will be killed by SIGTERM and restarted. If module in-mem size is larger than configured, it also will be restarted.

List of registered modules form every server sends automatically to each other server, doing both discovery and heartbeat tasks.

