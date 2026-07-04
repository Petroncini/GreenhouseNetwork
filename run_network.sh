#!/bin/bash

# Get directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$DIR"

echo "Compiling all components..."
make || { echo "Compilation failed!"; exit 1; }

cleanup() {
    echo ""
    echo "Stopping all processes..."

    pkill -TERM -x manager    2>/dev/null
    pkill -TERM -x sensor     2>/dev/null
    pkill -TERM -x actuator   2>/dev/null
    pkill -TERM -x controller 2>/dev/null

    # 1 second for clean up, then force-kill any still alive
    sleep 1
    pkill -KILL -x manager    2>/dev/null
    pkill -KILL -x sensor     2>/dev/null
    pkill -KILL -x actuator   2>/dev/null
    pkill -KILL -x controller 2>/dev/null

    echo "All processes stopped."
}

trap cleanup EXIT

echo "Compilation successful. Starting devices..."

run_in_terminal() {
    local cmd="$1"
    local title="$2"

    local wrapped="cd '$DIR' && $cmd; exit 0"

    if [[ "$OSTYPE" == "darwin"* ]]; then
        osascript <<EOF > /dev/null
tell application "Terminal"
    do script "cd '${DIR}' && ${cmd}"
end tell
EOF
    elif command -v gnome-terminal &> /dev/null; then
        gnome-terminal --title="$title" -- bash -c "$wrapped" &
    elif command -v xfce4-terminal &> /dev/null; then
        xfce4-terminal --title="$title" -e "bash -c \"$wrapped\"" &
    elif command -v konsole &> /dev/null; then
        konsole --title "$title" -e bash -c "$wrapped" &
    elif command -v xterm &> /dev/null; then
        xterm -T "$title" -e "bash -c \"$wrapped\"" &
    else
        echo "No supported GUI terminal emulator found. Starting '$title' in background..."
        bash -c "$wrapped" &
    fi
}

# Start Manager first and give it a moment to bind its ports
run_in_terminal "./manager" "Manager"
sleep 1

# Start Sensors (one of each type)
run_in_terminal "./sensor 1 temp" "Sensor - Temp (ID 1)"
run_in_terminal "./sensor 2 soil" "Sensor - Soil (ID 2)"
run_in_terminal "./sensor 3 co2"  "Sensor - CO2 (ID 3)"

# Start Actuators (one of each type)
run_in_terminal "./actuator 11 heater"     "Actuator - Heater (ID 11)"
run_in_terminal "./actuator 12 irrigation" "Actuator - Irrigation (ID 12)"
run_in_terminal "./actuator 13 injector"   "Actuator - Injector (ID 13)"
run_in_terminal "./actuator 14 cooler"     "Actuator - Cooler (ID 14)"

echo "All components spawned!"
echo "Run './controller' in a separate terminal to test the controller."
echo "--------------------------------------------------"
read -p "Press [Enter] to shut down all processes..."
