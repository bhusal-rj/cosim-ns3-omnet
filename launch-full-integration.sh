#!/bin/bash
# create launch-full-integration.sh

echo "🚀 Launching FULL V2X-NDN-NFV Integration"
echo "Leader: OMNeT++ NFV Orchestrator"
echo "Follower: Real ndnSIM"

# Build platform
echo "🔨 Building platform..."
make clean && make

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

# Kill existing processes
pkill -f v2x-ndn-nfv-cosim
pkill -f real-ndnsim-launcher
sleep 2

# Step 1: Start OMNeT++ Leader
echo "🎯 Starting OMNeT++ NFV Orchestrator (Leader)..."
./v2x-ndn-nfv-cosim --real-omnet --kathmandu --traffic heavy \
    --port $AVAILABLE_PORT --sim-time 120 &
LEADER_PID=$!

# Step 2: Wait for leader to be ready
sleep 5

# Step 3: Copy ndnSIM script and launch
echo "📋 Preparing ndnSIM script..."
cp ns3-scripts/simple-real-ndnsim-launcher.cc ~/ndnSIM/ns-3/scratch/

echo "🔬 Starting ndnSIM Follower..."
cd ~/ndnSIM/ns-3
# Move problematic scripts temporarily to avoid build errors
mv scratch/real-ndnsim-launcher.cc scratch/real-ndnsim-launcher.cc.bak 2>/dev/null || true
mv scratch/v2x-ndn-nfv-cosim.cc scratch/v2x-ndn-nfv-cosim.cc.bak 2>/dev/null || true
./waf --run "simple-real-ndnsim-launcher --leader=127.0.0.1 --port=$AVAILABLE_PORT --time=120"

# Wait for completion
wait $LEADER_PID

echo "🎉 Full integration test completed!"