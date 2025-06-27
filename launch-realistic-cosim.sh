#!/bin/bash
# create launch-realistic-cosim.sh

echo "🚀 Launching V2X-NDN-NFV Co-simulation Platform"
echo "Leader-Follower Architecture: OMNeT++ ↔ ndnSIM"

# Find available port
AVAILABLE_PORT=$(python3 -c "
import socket
s = socket.socket()
s.bind(('', 0))
port = s.getsockname()[1]
s.close()
print(port)
")

echo "📡 Using port: $AVAILABLE_PORT"

# Kill any existing processes
pkill -f v2x-ndn-nfv-cosim
pkill -f real-ndnsim-launcher
sleep 2

# Build if needed
echo "🔨 Building co-simulation platform..."
make clean && make

# Start OMNeT++ Leader in background
echo "🎯 Starting OMNeT++ NFV Orchestrator (Leader)..."
./v2x-ndn-nfv-cosim --real-omnet --kathmandu --traffic heavy \
    --port $AVAILABLE_PORT --sim-time 120 &
LEADER_PID=$!

# Wait for leader to start
sleep 5

# Start ndnSIM Follower
echo "🔬 Starting ndnSIM Follower..."
cd ~/ndnSIM/ns-3
./waf --run "real-ndnsim-launcher --leader=127.0.0.1 --port=$AVAILABLE_PORT --time=120"

# Wait for completion
wait $LEADER_PID

echo "🎉 Co-simulation completed!"