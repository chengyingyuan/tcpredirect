#!/bin/bash
#
sudo [ -f  /etc/systemd/system/tcpredirect.service ] && sudo rm -f /etc/systemd/system/tcpredirect.service
sudo cp tcpredirect.service /etc/systemd/system/tcpredirect.service
sudo systemctl daemon-reload
sudo systemctl start tcpredirect
sleep 1
sudo systemctl status tcpredirect

