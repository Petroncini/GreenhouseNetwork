#!/bin/bash

echo "Stopping all manager, sensor, and actuator processes..."
pkill -x manager 2>/dev/null
pkill -x sensor 2>/dev/null
pkill -x actuator 2>/dev/null
echo "All processes stopped."
