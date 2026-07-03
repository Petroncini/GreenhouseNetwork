#!/bin/bash

echo "Stopping all processes..."
pkill -TERM -x manager    2>/dev/null
pkill -TERM -x sensor     2>/dev/null
pkill -TERM -x actuator   2>/dev/null
pkill -TERM -x controller 2>/dev/null
sleep 1
pkill -KILL -x manager    2>/dev/null
pkill -KILL -x sensor     2>/dev/null
pkill -KILL -x actuator   2>/dev/null
pkill -KILL -x controller 2>/dev/null
echo "All processes stopped."
