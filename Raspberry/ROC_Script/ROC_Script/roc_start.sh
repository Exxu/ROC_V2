#!/usr/bin/env bash

export TERM=xterm

screen -dmS roc_rtk_inject bash -lc "cd /home/roc/ROC_V2/RTK_infrastructure/ && python3 rtcm_injector.py; exec bash"

screen -dmS roc_tool_radio bash -lc "cd /home/roc/ROC_V2/RC_Tool_Radio_Bridge/build && ./roc_rc_tool_radio_bridge; exec bash"

screen -dmS roc_zrok_ssh bash -lc "zrok2 share private --headless --share-token roc-ssh 127.0.0.1:22"

screen -dmS roc_zrok_mavlink bash -lc "zrok2 share private --headless --share-token roc-mavlink 127.0.0.1:14540"
