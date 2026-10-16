/*
 * PriorityGuard Unit Tests
 * Tests for scheduling, mutex management, PIP, PCP, and inversion detection.
 */

#include "../include/types.h"
#include "../include/task.h"
#include "../include/mutex_manager.h"
#include "../include/scheduler.h"
#include "../include/detector.h"
#include "../include/workload_generator.h"
#include "../include/recommender.h"

#include <iostream>
#include <cassert>
#include <string>

using namespace pg;

void testClassicPriorityInversion() {
    std::cout << "[TEST] Running testClassicPriorityInversion..." << std::endl;
    WorkloadConfig wc = WorkloadGenerator::classicInversion();
    
    // 1. Run Baseline (No Inheritance)
    wc.protocol = Protocol::NONE;
    Scheduler sched_none;
    sched_none.init(wc);
    SimulationResults res_none = sched_none.run();
    InversionDetector detector;
    res_none.inversions = detector.detect(res_none.events, res_none.task_results);
    
    // In baseline, priority inversion MUST be detected
    assert(!res_none.inversions.empty());
    assert(res_none.inversions[0].high_task_id == "TH");
    assert(res_none.inversions[0].low_task_id == "TL");
    assert(res_none.inversions[0].interference_time > 0);
    std::cout << "  -> Baseline correctly detected inversion with interference=" 
              << res_none.inversions[0].interference_time << " ticks" << std::endl;

    // 2. Run PIP (Priority Inheritance Protocol)
    wc.protocol = Protocol::PIP;
    Scheduler sched_pip;
    sched_pip.init(wc);
    SimulationResults res_pip = sched_pip.run();
    res_pip.inversions = detector.detect(res_pip.events, res_pip.task_results);
    
    // In PIP, Task L inherits priority 10, so Task M cannot preempt L while holding R1.
    // Therefore, Task H finishes earlier than in baseline!
    int th_resp_none = 0, th_resp_pip = 0;
    for (const auto& tr : res_none.task_results) {
        if (tr.task_id == "TH") th_resp_none = tr.response_time;
    }
    for (const auto& tr : res_pip.task_results) {
        if (tr.task_id == "TH") th_resp_pip = tr.response_time;
    }
    
    std::cout << "  -> Task H response time: Baseline=" << th_resp_none << ", PIP=" << th_resp_pip << std::endl;
    assert(th_resp_pip < th_resp_none);
    assert(res_pip.priority_inheritance_events > 0);
    std::cout << "  -> PIP test PASSED." << std::endl;
}

void testNoInversionScenario() {
    std::cout << "[TEST] Running testNoInversionScenario..." << std::endl;
    WorkloadConfig wc = WorkloadGenerator::noInversion();
    wc.protocol = Protocol::NONE;
    
    Scheduler sched;
    sched.init(wc);
    SimulationResults res = sched.run();
    InversionDetector detector;
    res.inversions = detector.detect(res.events, res.task_results);
    
    // Must report NO inversions
    int interference_inversions = 0;
    for (const auto& inv : res.inversions) {
        if (inv.interference_time > 0) interference_inversions++;
    }
    assert(interference_inversions == 0);
    std::cout << "  -> No-inversion test PASSED." << std::endl;
}

void testPriorityCeilingProtocol() {
    std::cout << "[TEST] Running testPriorityCeilingProtocol..." << std::endl;
    WorkloadConfig wc = WorkloadGenerator::classicInversion();
    wc.protocol = Protocol::PCP;
    
    Scheduler sched;
    sched.init(wc);
    
    // Verify ceiling of R1 is computed to be 10 (highest priority of task using it, TH=10, TL=1)
    assert(sched.mutex_mgr.getCeiling("R1") == 10);
    
    SimulationResults res = sched.run();
    assert(res.completed_tasks == 3);
    std::cout << "  -> PCP test PASSED." << std::endl;
}

void testNestedBlocking() {
    std::cout << "[TEST] Running testNestedBlocking..." << std::endl;
    WorkloadConfig wc = WorkloadGenerator::nestedBlocking();
    wc.protocol = Protocol::PIP;
    
    Scheduler sched;
    sched.init(wc);
    SimulationResults res = sched.run();
    
    assert(res.completed_tasks == (int)wc.tasks.size());
    std::cout << "  -> Nested blocking test PASSED." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "PriorityGuard Core C++ Unit Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        testClassicPriorityInversion();
        testNoInversionScenario();
        testPriorityCeilingProtocol();
        testNestedBlocking();
        std::cout << "========================================" << std::endl;
        std::cout << "ALL C++ TESTS PASSED SUCCESSFULLY! (4/4)" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failure with exception: " << e.what() << std::endl;
        return 1;
    }
}

