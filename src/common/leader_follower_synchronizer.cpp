/*
Leader-Follower Synchronizer Implementation
*/

#include "leader_follower_synchronizer.h"
#include "../adapters/omnet_orchestrator.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace cosim {

LeaderFollowerSynchronizer::LeaderFollowerSynchronizer(const Config& config)
    : config_(config), leader_(nullptr), follower_(nullptr),
      currentTime_(0.0), running_(false), initialized_(false) {
    
    // Initialize performance metrics
    performanceMetrics_.totalSteps = 0;
    performanceMetrics_.successfulSteps = 0;
    performanceMetrics_.failedSteps = 0;
    performanceMetrics_.avgStepDuration = 0.0;
    performanceMetrics_.maxStepDuration = 0.0;
    performanceMetrics_.minStepDuration = std::numeric_limits<double>::max();
    performanceMetrics_.timeouts = 0;
}

LeaderFollowerSynchronizer::~LeaderFollowerSynchronizer() {
    stop();
}

void LeaderFollowerSynchronizer::setLeader(SimulatorInterface* leader) {
    leader_ = leader;
    std::cout << "👑 Leader set: " << (leader ? "OMNeT++ Orchestrator" : "None") << std::endl;
}

void LeaderFollowerSynchronizer::setFollower(SimulatorInterface* follower) {
    follower_ = follower;
    std::cout << "👥 Follower set: " << (follower ? "NS-3/ndnSIM" : "None") << std::endl;
}

bool LeaderFollowerSynchronizer::initialize() {
    if (!leader_ || !follower_) {
        std::cerr << "❌ Both leader and follower must be set before initialization" << std::endl;
        return false;
    }
    
    std::cout << "🔧 Initializing Leader-Follower Co-simulation..." << std::endl;
    
    // Initialize leader first (OMNeT++ orchestrator)
    std::cout << "🎯 Initializing Leader (OMNeT++)..." << std::endl;
    if (!leader_->initialize()) {
        std::cerr << "❌ Failed to initialize leader" << std::endl;
        return false;
    }
    
    // Give leader time to start server
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Initialize follower (ndnSIM)
    std::cout << "🎯 Initializing Follower (ndnSIM)..." << std::endl;
    if (!follower_->initialize()) {
        std::cerr << "❌ Failed to initialize follower" << std::endl;
        return false;
    }
    
    // Wait for connection establishment
    std::cout << "🔗 Waiting for leader-follower connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Verify both simulators are ready
    if (!leader_->isRunning() || !follower_->isRunning()) {
        std::cerr << "❌ One or both simulators failed to start properly" << std::endl;
        return false;
    }
    
    initialized_ = true;
    currentTime_ = 0.0;
    performanceMetrics_.startTime = std::chrono::steady_clock::now();
    
    std::cout << "✅ Leader-Follower co-simulation initialized successfully" << std::endl;
    return true;
}

bool LeaderFollowerSynchronizer::run() {
    if (!initialized_) {
        std::cerr << "❌ Synchronizer not initialized" << std::endl;
        return false;
    }
    
    running_ = true;
    double syncInterval = config_.getSyncInterval();
    double simulationTime = config_.getSimulationTime();
    
    std::cout << "🚀 Starting Leader-Follower co-simulation..." << std::endl;
    std::cout << "⏱️  Duration: " << simulationTime << "s, Sync interval: " 
              << (syncInterval * 1000) << "ms" << std::endl;
    
    int stepCount = 0;
    auto lastProgressTime = std::chrono::steady_clock::now();
    
    while (running_ && currentTime_ < simulationTime) {
        auto stepStart = std::chrono::steady_clock::now();
        
        // Execute synchronized time step
        bool stepSuccess = executeTimeStep();
        
        auto stepEnd = std::chrono::steady_clock::now();
        auto stepDuration = std::chrono::duration<double>(stepEnd - stepStart).count();
        
        // Update metrics
        updatePerformanceMetrics();
        logTimeStep(stepDuration);
        
        if (!stepSuccess) {
            std::cerr << "⚠️ Time step failed at t=" << currentTime_ << "s" << std::endl;
            handleSynchronizationError();
            
            if (performanceMetrics_.failedSteps > 5) {
                std::cerr << "❌ Too many failed steps, terminating simulation" << std::endl;
                break;
            }
        } else {
            currentTime_.store(currentTime_.load() + syncInterval);
            stepCount++;
        }
        
        // Progress reporting every 10 seconds
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastProgressTime).count() >= 10.0) {
            double progress = (currentTime_ / simulationTime) * 100.0;
            std::cout << "📊 Progress: " << std::fixed << std::setprecision(1) << progress 
                      << "% (t=" << std::setprecision(2) << currentTime_ 
                      << "s, step=" << stepCount << ")" << std::endl;
            lastProgressTime = now;
        }
        
        // Check if simulators are still running
        if (!leader_->isRunning() || !follower_->isRunning()) {
            std::cout << "⚠️ One of the simulators stopped running" << std::endl;
            break;
        }
    }
    
    performanceMetrics_.endTime = std::chrono::steady_clock::now();
    running_ = false;
    
    if (currentTime_ >= simulationTime) {
        std::cout << "✅ Co-simulation completed successfully!" << std::endl;
        return true;
    } else {
        std::cout << "⚠️ Co-simulation terminated early" << std::endl;
        return false;
    }
}

bool LeaderFollowerSynchronizer::executeTimeStep() {
    double syncInterval = config_.getSyncInterval();
    
    try {
        // Step 1: Leader advances time and sends sync command
        if (!leader_->step(syncInterval)) {
            std::cerr << "❌ Leader step failed" << std::endl;
            performanceMetrics_.failedSteps++;
            return false;
        }
        
        // Step 2: Follower receives sync command and advances
        if (!follower_->step(syncInterval)) {
            std::cerr << "❌ Follower step failed" << std::endl;
            performanceMetrics_.failedSteps++;
            return false;
        }
        
        // Step 3: Exchange vehicle data between simulators
        auto leaderVehicles = leader_->getVehicleData();
        auto followerVehicles = follower_->getVehicleData();
        
        // Update vehicle data in both simulators
        follower_->updateVehicleData(leaderVehicles);
        leader_->updateVehicleData(followerVehicles);
        
        performanceMetrics_.successfulSteps++;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception during time step: " << e.what() << std::endl;
        performanceMetrics_.failedSteps++;
        return false;
    }
}

void LeaderFollowerSynchronizer::handleSynchronizationError() {
    std::cout << "🔧 Handling synchronization error..." << std::endl;
    
    // Give simulators time to recover
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check simulator states
    if (!leader_->isRunning()) {
        std::cerr << "❌ Leader (OMNeT++) is not running" << std::endl;
    }
    
    if (!follower_->isRunning()) {
        std::cerr << "❌ Follower (ndnSIM) is not running" << std::endl;
    }
}

void LeaderFollowerSynchronizer::updatePerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceMetrics_.totalSteps++;
}

void LeaderFollowerSynchronizer::logTimeStep(double stepDuration) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    // Update step duration statistics
    performanceMetrics_.avgStepDuration = 
        (performanceMetrics_.avgStepDuration * (performanceMetrics_.totalSteps - 1) + stepDuration) / 
        performanceMetrics_.totalSteps;
    
    performanceMetrics_.maxStepDuration = std::max(performanceMetrics_.maxStepDuration, stepDuration);
    performanceMetrics_.minStepDuration = std::min(performanceMetrics_.minStepDuration, stepDuration);
}

void LeaderFollowerSynchronizer::stop() {
    if (!running_) return;
    
    std::cout << "🛑 Stopping co-simulation..." << std::endl;
    
    running_ = false;
    
    // Shutdown simulators
    if (follower_) {
        follower_->shutdown();
    }
    
    if (leader_) {
        leader_->shutdown();
    }
    
    std::cout << "✅ Co-simulation stopped" << std::endl;
}

void LeaderFollowerSynchronizer::printPerformanceSummary() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(metricsMutex_));
    
    auto totalDuration = std::chrono::duration<double>(
        performanceMetrics_.endTime - performanceMetrics_.startTime).count();
    
    std::cout << "\n📊 === Co-simulation Performance Summary ===" << std::endl;
    std::cout << "⏱️  Total wall clock time: " << std::fixed << std::setprecision(2) 
              << totalDuration << " seconds" << std::endl;
    std::cout << "🎯 Simulation time: " << currentTime_.load() << " seconds" << std::endl;
    std::cout << "⚡ Time ratio: " << std::setprecision(1) 
              << (currentTime_.load() / totalDuration) << "x real-time" << std::endl;
    
    std::cout << "\n📈 Step Statistics:" << std::endl;
    std::cout << "  Total steps: " << performanceMetrics_.totalSteps << std::endl;
    std::cout << "  Successful steps: " << performanceMetrics_.successfulSteps << std::endl;
    std::cout << "  Failed steps: " << performanceMetrics_.failedSteps << std::endl;
    std::cout << "  Success rate: " << std::setprecision(1) 
              << (100.0 * performanceMetrics_.successfulSteps / performanceMetrics_.totalSteps) 
              << "%" << std::endl;
    
    std::cout << "\n⏲️  Step Timing:" << std::endl;
    std::cout << "  Average step duration: " << std::setprecision(3) 
              << (performanceMetrics_.avgStepDuration * 1000) << " ms" << std::endl;
    std::cout << "  Min step duration: " 
              << (performanceMetrics_.minStepDuration * 1000) << " ms" << std::endl;
    std::cout << "  Max step duration: " 
              << (performanceMetrics_.maxStepDuration * 1000) << " ms" << std::endl;
    
    if (performanceMetrics_.timeouts > 0) {
        std::cout << "⚠️  Timeouts: " << performanceMetrics_.timeouts << std::endl;
    }
    
    std::cout << "============================================\n" << std::endl;
}

void LeaderFollowerSynchronizer::exportPerformanceData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open performance export file: " << filename << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(metricsMutex_));
    
    auto totalDuration = std::chrono::duration<double>(
        performanceMetrics_.endTime - performanceMetrics_.startTime).count();
    
    file << "# V2X-NDN-NFV Co-simulation Performance Data\n";
    file << "# Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
    file << "\n";
    file << "simulation_time," << currentTime_.load() << "\n";
    file << "wall_clock_time," << totalDuration << "\n";
    file << "time_ratio," << (currentTime_.load() / totalDuration) << "\n";
    file << "total_steps," << performanceMetrics_.totalSteps << "\n";
    file << "successful_steps," << performanceMetrics_.successfulSteps << "\n";
    file << "failed_steps," << performanceMetrics_.failedSteps << "\n";
    file << "success_rate," << (100.0 * performanceMetrics_.successfulSteps / performanceMetrics_.totalSteps) << "\n";
    file << "avg_step_duration_ms," << (performanceMetrics_.avgStepDuration * 1000) << "\n";
    file << "min_step_duration_ms," << (performanceMetrics_.minStepDuration * 1000) << "\n";
    file << "max_step_duration_ms," << (performanceMetrics_.maxStepDuration * 1000) << "\n";
    file << "timeouts," << performanceMetrics_.timeouts << "\n";
    
    file.close();
    std::cout << "📁 Performance data exported to: " << filename << std::endl;
}

} // namespace cosim
