ModBusServer
============

This is sample of ModBusServer part. Only for evaluation purposes, not for any other use - commercial or free.
Absolutely no warranty of work for any parts. 


ModBusServer is a part of more complex system I have involved in development some time ago. The goal of ModBusServer creation was a requirement of easy use platform that able to process any kind of events by locally hosted modules written on any language with queue-like server-free messaging system, highly scalable and robust. Based on ZeroMQ high-level API and MessagePack binary packing functions, it provide easy configurable, developing & supporting bridge between Nginx web server and FCGI-like modules, with additional features like load balancing, timeouts processing, automated self-discovering, modules monitoring and so on.
