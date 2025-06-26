/*
Leader-Follower Synchronizer for V2X-NDN-NFV Co-simulation
Implements the synchronization protocol between OMNeT++ (Leader) and ndnSIM (Follower)
*/

#ifndef LEADER_FOLLOWER_SYNCHRONIZER_H
#define LEADER_FOLLOWER_SYNCHRONIZER_H

#include "synchronizer.h"
#include "config.h"
#include "message.h"
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace cosim {

// Forward declarations
class OMNeTOrchestrator;

class LeaderFollowerSynchronizer {
public:
    LeaderFollowerSynchronizer(const Config& config);
    ~LeaderFollowerSynchronizer();
    
    // Setup
    void setLeader(SimulatorInterface* leader);
    void setFollower(SimulatorInterface* follower);
    
    // Control
    bool initialize();
    bool run();
    void stop();
    
    // Status
    double getCurrentTime() const { return currentTime_.load(); }
    bool isRunning() const { return running_.load(); }
    
    // Performance monitoring
    void printPerformanceSummary() const;
    void exportPerformanceData(const std::string& filename) const;
    
private:
    // Synchronization loop
    bool executeTimeStep();
    void handleSynchronizationError();
    
    // Performance tracking
    void updatePerformanceMetrics();
    void logTimeStep(double stepDuration);
    
    Config config_;
    
    // Simulator references
    SimulatorInterface* leader_;
    SimulatorInterface* follower_;
    
    // Synchronization state
    std::atomic<double> currentTime_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Performance metrics
    struct SyncPerformanceMetrics {
        uint64_t totalSteps;
        uint64_t successfulSteps;
        uint64_t failedSteps;
        double avgStepDuration;
        double maxStepDuration;
        double minStepDuration;
        uint64_t timeouts;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
    } performanceMetrics_;
    
    std::mutex metricsMutex_;
};

} // namespace cosim

#endif // LEADER_FOLLOWER_SYNCHRONIZER_H
