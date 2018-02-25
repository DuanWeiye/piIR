#!/bin/sh

#sudo killall lircd
#sudo lircd --device /dev/lirc0

ps aux | grep lircd | grep lirc0 >/dev/null 2>/dev/null
if [ $? -eq 0 ]
then
  echo "Lirc Service OK"
else
  echo "Lirc Service Down"

  sudo killall lircd >/dev/null 2>/dev/null
  sudo lircd --device /dev/lirc0
fi

irsend SEND_ONCE room tv_power
sleep 15
irsend SEND_ONCE room tv_1
