#!/bin/bash

# Get directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$DIR"

echo "Compiling manager, sensor, and actuator..."
g++ -std=c++17 -pthread manager.cpp -o manager
g++ -std=c++17 -pthread sensor.cpp -o sensor
g++ -std=c++17 -pthread actuator.cpp -o actuator

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

cleanup() {
    echo ""
    echo "Stopping all manager, sensor, and actuator processes..."
    pkill -x manager 2>/dev/null
    pkill -x sensor 2>/dev/null
    pkill -x actuator 2>/dev/null
    echo "All processes stopped."
}

# Trap Ctrl+C (SIGINT) and exit (SIGTERM) to clean up
trap cleanup EXIT

echo "Compilation successful. Starting devices..."

run_in_terminal() {
    local cmd="$1"
    local title="$2"

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS: Use AppleScript with the default Terminal application
        osascript -e "tell application \"Terminal\" to do script \"cd '$DIR' && echo -n -e '\\\\033]0;${title}\\\\007' && clear && ${cmd}\"" > /dev/null
    elif command -v gnome-terminal &> /dev/null; then
        gnome-terminal --title="$title" -- bash -c "cd '$DIR' && ${cmd}" &
    elif command -v xfce4-terminal &> /dev/null; then
        xfce4-terminal --title="$title" -e "bash -c \"cd '$DIR' && ${cmd}\"" &
    elif command -v konsole &> /dev/null; then
        konsole --title "$title" -e bash -c "cd '$DIR' && ${cmd}" &
    elif command -v xterm &> /dev/null; then
        xterm -T "$title" -e "bash -c \"cd '$DIR' && ${cmd}\"" &
    else
        # Fallback to running in the background of the current terminal if no GUI emulator is installed
        echo "No supported GUI terminal emulator found. Starting '$title' in background..."
        bash -c "cd '$DIR' && ${cmd}" &
    fi
}

# Start Manager
run_in_terminal "./manager" "Manager"

# Give manager a moment to initialize before connecting devices
sleep 1

# Start Sensors (each of different type)
run_in_terminal "./sensor 1 temp" "Sensor - Temp (ID 1)"
run_in_terminal "./sensor 2 soil" "Sensor - Soil (ID 2)"
run_in_terminal "./sensor 3 co2" "Sensor - CO2 (ID 3)"

# Start Actuators (each of different type)
run_in_terminal "./actuator 11 heater" "Actuator - Heater (ID 11)"
run_in_terminal "./actuator 12 irrigation" "Actuator - Irrigation (ID 12)"
run_in_terminal "./actuator 13 injector" "Actuator - Injector (ID 13)"
run_in_terminal "./actuator 14 cooler" "Actuator - Cooler (ID 14)"

echo "All components spawned!"
echo "--------------------------------------------------"
read -p "Press [Enter] to shut down all processes..."
