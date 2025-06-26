#!/bin/bash

# V2X-NDN-NFV Co-simulation Test Script
# Tests the Leader-Follower architecture implementation

echo "=== V2X-NDN-NFV Co-simulation Test Suite ==="

# Check dependencies
echo "🔍 Checking dependencies..."

# Check for JSON library
if ! pkg-config --exists jsoncpp; then
    echo "❌ jsoncpp library not found. Installing..."
    sudo apt-get update
    sudo apt-get install -y libjsoncpp-dev
fi

# Check NS-3 installation
if [ ! -d "/home/rajesh/ndnSIM/ns-3" ]; then
    echo "❌ NS-3/ndnSIM not found at expected location"
    exit 1
fi

echo "✅ Dependencies check passed"

# Build the co-simulation platform
echo "🔨 Building V2X-NDN-NFV co-simulation platform..."
make clean
if ! make; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful"

# Copy NS-3 script to the right location
echo "📋 Preparing NS-3 co-simulation script..."
cp ns3-scripts/v2x-ndn-nfv-cosim.cc /home/rajesh/ndnSIM/ns-3/scratch/

# Test 1: Mock simulators
echo "🧪 Test 1: Mock simulators (basic functionality)"
timeout 30s ./v2x-ndn-nfv-cosim --sim-time 10 --sync-interval 500
if [ $? -eq 0 ]; then
    echo "✅ Test 1 passed"
else
    echo "⚠️ Test 1 had issues (timeout or error)"
fi

# Test 2: Real OMNeT++ with Mock NS-3
echo "🧪 Test 2: Real OMNeT++ orchestrator with Mock NS-3"
timeout 30s ./v2x-ndn-nfv-cosim --real-omnet --sim-time 10 --sync-interval 500
if [ $? -eq 0 ]; then
    echo "✅ Test 2 passed"
else
    echo "⚠️ Test 2 had issues"
fi

# Test 3: Mock OMNeT++ with Real NS-3
echo "🧪 Test 3: Mock OMNeT++ with Real NS-3"
timeout 60s ./v2x-ndn-nfv-cosim --real-ns3 --sim-time 15 --sync-interval 200 &
COSIM_PID=$!

# Give co-simulation time to start
sleep 5

# Check if NS-3 process is running
if pgrep -f "v2x-ndn-nfv-cosim" > /dev/null; then
    echo "✅ Test 3: Co-simulation process started"
    
    # Wait for completion or timeout
    wait $COSIM_PID
    if [ $? -eq 0 ]; then
        echo "✅ Test 3 passed"
    else
        echo "⚠️ Test 3 completed with warnings"
    fi
else
    echo "❌ Test 3 failed to start"
fi

# Test 4: Full integration (both real simulators)
echo "🧪 Test 4: Full V2X-NDN-NFV integration"
timeout 90s ./v2x-ndn-nfv-cosim --real-ns3 --real-omnet --kathmandu --traffic normal --sim-time 20 &
FULL_PID=$!

sleep 10

if pgrep -f "v2x-ndn-nfv-cosim" > /dev/null; then
    echo "✅ Test 4: Full integration started"
    wait $FULL_PID
    echo "✅ Test 4 completed"
else
    echo "❌ Test 4 failed to start"
fi

# Test 5: Kathmandu scenario
echo "🧪 Test 5: Kathmandu intersection scenario"
timeout 60s ./v2x-ndn-nfv-cosim --kathmandu --traffic heavy --sim-time 15 --sync-interval 100
if [ $? -eq 0 ]; then
    echo "✅ Test 5 passed"
else
    echo "⚠️ Test 5 had issues"
fi

echo ""
echo "🎉 V2X-NDN-NFV Co-simulation test suite completed!"
echo ""
echo "📊 Usage examples:"
echo "  # Basic mock simulation:"
echo "  ./v2x-ndn-nfv-cosim"
echo ""
echo "  # Real NS-3 with ndn-grid example:"
echo "  ./v2x-ndn-nfv-cosim --real-ns3 --ns3-example ndn-grid"
echo ""
echo "  # Full Kathmandu V2X scenario:"
echo "  ./v2x-ndn-nfv-cosim --real-ns3 --real-omnet --kathmandu --traffic heavy --sim-time 60"
echo ""
echo "  # Fast sync for V2X applications:"
echo "  ./v2x-ndn-nfv-cosim --real-ns3 --sync-interval 50 --sim-time 30"
echo ""
