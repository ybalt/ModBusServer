#!/bin/sh

NAME="modbusd"
DIRROOT=$(cd $(dirname $0) && pwd)
DAEMON="$DIRROOT/$NAME"
CONFIG="$DIRROOT/test.conf"
PIDS=`ps -aef | grep "$NAME " | grep -v grep | awk '{print $2}'`

if [ ! -z $2 ]
then
	if [ ! -f $2 ]
	then
		echo "Config file $2 not found, exit.."
		exit 0
	else
		CONFIG=$2
	fi
fi

start() {
    echo -n "Starting $NAME, using config file $CONFIG: \n\n"
    ./$NAME $CONFIG &
    if [ ! -z "$PIDS" ]
	then
		echo "Ok"
	fi
}  
stop() {
	if  [ ! -z "$PIDS" ]
	then
		echo -n "Stopping $NAME: "
		for i in $PIDS; do
			echo "\nkilling process $i"
			kill $i
		done
		echo "Ok."
	else
		echo "Cannot stop, $NAME is not running"
	fi
}
restart() {
    stop
	echo ""
    sleep 2
	echo ""
    start
}

case "$1" in
    start)
        start
;;
    stop)
        stop
;;
    restart)
        restart
;;
    status)
        if [ ! -z "$PIDS" ]
        then
            echo "$NAME is running:"
			for i in $PIDS; do
				echo `ps -aef | grep "$NAME " | grep -v grep`
			done
            exit 0
        fi
        echo "$NAME is not running."
        exit 3
;;
    *)
        echo "Script usage: \n\tstart [config] \n\tstatus \n\tstop \n\trestart [config]"
        exit 1
;;
esac
exit 0