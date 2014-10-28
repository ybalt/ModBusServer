#!/bin/bash

valgrind --tool=callgrind --separate-threads=yes ./modbusd ./mod1.conf
