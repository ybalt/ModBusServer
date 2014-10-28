#!/bin/bash

valgrind --tool=memcheck --track-origins=yes --suppressions=modb.supp --leak-check=full --log-file=mod1.val.log ./modbusd ./mod1.conf
