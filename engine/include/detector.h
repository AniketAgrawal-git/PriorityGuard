/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * detector.h - Priority inversion detection and analysis
 *
 * Analyzes simulation event logs to identify priority inversion scenarios.
 * Detection criteria:
 *   1. A high-priority task H is BLOCKED waiting for a resource held by 
 *      a lower-priority task L.
 *   2. During the blocking period, an intermediate-priority task M 
 *      (priority between L and H) preempts L, extending H's blocking time.
 *
 * Direct blocking (H waits for L) is distinguished from the additional
 * delay caused by intermediate-priority interference (priority inversion).
 */

#ifndef PRIORITYGUARD_DETECTOR_H
#define PRIORITYGUARD_DETECTOR_H

#include "types.h"
#include <vector>
#include <string>
#include <map>
#include <sstream>

namespace pg {

class InversionDetector {
public:
    // Analyze simulation events and task results to find priority inversions
    std::vector<InversionRecord> detect(const std::vector<SimEvent>& events,
                                         const std::vector<TaskResult>& task_results) {
        std::vector<InversionRecord> inversions;
        
        // Track blocking intervals: when a task blocks and when it unblocks
        struct BlockingInterval {
            std::string blocked_task_id;
            int blocked_task_priority;
            std::string resource_id;
            std::string owner_task_id;
            int owner_base_priority;
            int start_time;
            int end_time;
        };
        
        std::vector<BlockingInterval> blocking_intervals;
        
        // Build a map of task base priorities
        std::map<std::string, int> task_priorities;
        for (const auto& tr : task_results) {
            task_priorities[tr.task_id] = tr.base_priority;
        }
        
        // Parse events to find blocking intervals
        // Track active blocks: task_id -> BlockingInterval
        std::map<std::string, BlockingInterval> active_blocks;
        
        for (const auto& evt : events) {
            if (evt.type == EventType::TASK_BLOCKED) {
                BlockingInterval bi;
                bi.blocked_task_id = evt.task_id;
                bi.blocked_task_priority = evt.base_priority;
                bi.resource_id = evt.resource_id;
                bi.start_time = evt.time;
                bi.end_time = -1;
                
                // Find who owns the resource (from the details or preceding events)
                bi.owner_task_id = extractOwnerFromDetails(evt.details);
                if (task_priorities.find(bi.owner_task_id) != task_priorities.end()) {
                    bi.owner_base_priority = task_priorities[bi.owner_task_id];
                } else {
                    bi.owner_base_priority = 0;
                }
                
                active_blocks[evt.task_id] = bi;
            }
            else if (evt.type == EventType::TASK_UNBLOCKED) {
                auto it = active_blocks.find(evt.task_id);
                if (it != active_blocks.end()) {
                    it->second.end_time = evt.time;
                    blocking_intervals.push_back(it->second);
                    active_blocks.erase(it);
                }
            }
        }
        
        // Handle blocks that never ended (task never unblocked)
        for (auto& pair : active_blocks) {
            // Find end of simulation
            int sim_end = 0;
            for (const auto& e : events) {
                if (e.time > sim_end) sim_end = e.time;
            }
            pair.second.end_time = sim_end;
            blocking_intervals.push_back(pair.second);
        }
        
        // For each blocking interval, check if it's a priority inversion
        for (auto& bi : blocking_intervals) {
            // Priority inversion requires:
            // 1. Blocked task has HIGHER priority than the resource owner
            if (bi.blocked_task_priority <= bi.owner_base_priority) {
                continue; // Not an inversion - blocked by equal or higher priority task
            }
            
            // This is at least direct blocking (high blocks on low)
            // Now check if intermediate tasks preempted the owner during this interval
            std::vector<std::string> interfering_tasks;
            int interference_time = 0;
            
            // Find execution events during the blocking interval
            // An interfering task M has: owner_priority < M_priority < blocked_priority
            // and M executes during [bi.start_time, bi.end_time]
            std::map<std::string, int> interference_durations;
            
            for (const auto& evt : events) {
                if (evt.time < bi.start_time || evt.time > bi.end_time) continue;
                
                // Look for tasks that start/resume during this interval
                if (evt.type == EventType::TASK_START || 
                    evt.type == EventType::TASK_RESUME) {
                    std::string mid_task = evt.task_id;
                    if (mid_task == bi.blocked_task_id || mid_task == bi.owner_task_id) continue;
                    
                    int mid_priority = 0;
                    auto pit = task_priorities.find(mid_task);
                    if (pit != task_priorities.end()) mid_priority = pit->second;
                    
                    // Check if this is an intermediate priority task
                    if (mid_priority > bi.owner_base_priority && 
                        mid_priority < bi.blocked_task_priority) {
                        if (std::find(interfering_tasks.begin(), interfering_tasks.end(), 
                                     mid_task) == interfering_tasks.end()) {
                            interfering_tasks.push_back(mid_task);
                        }
                    }
                }
            }
            
            // Calculate interference time by counting time units where an
            // intermediate task was running instead of the owner
            for (int t = bi.start_time; t < bi.end_time; t++) {
                // Find what was running at time t
                std::string running_at_t = findRunningTaskAtTime(events, t);
                if (!running_at_t.empty() && 
                    running_at_t != bi.owner_task_id && 
                    running_at_t != bi.blocked_task_id) {
                    int mid_prio = 0;
                    auto pit = task_priorities.find(running_at_t);
                    if (pit != task_priorities.end()) mid_prio = pit->second;
                    
                    if (mid_prio > bi.owner_base_priority && 
                        mid_prio < bi.blocked_task_priority) {
                        interference_time++;
                    }
                }
            }
            
            int total_blocking = bi.end_time - bi.start_time;
            int direct_blocking = total_blocking - interference_time;
            
            // Build inversion record
            InversionRecord inv;
            inv.start_time = bi.start_time;
            inv.end_time = bi.end_time;
            inv.direct_blocking_time = direct_blocking;
            inv.interference_time = interference_time;
            inv.high_task_id = bi.blocked_task_id;
            inv.low_task_id = bi.owner_task_id;
            inv.resource_id = bi.resource_id;
            inv.interfering_tasks = interfering_tasks;
            
            // Check if this caused a deadline miss
            inv.caused_deadline_miss = false;
            for (const auto& tr : task_results) {
                if (tr.task_id == bi.blocked_task_id && tr.deadline_missed) {
                    inv.caused_deadline_miss = true;
                    break;
                }
            }
            
            // Build causal chain explanation
            inv.causal_chain = buildCausalChain(inv);
            
            // Only report as priority inversion if there was actual
            // intermediate-task interference OR if high-prio blocked on low-prio resource
            // (Direct blocking by lower-priority task is already noteworthy)
            inversions.push_back(inv);
        }
        
        return inversions;
    }

private:
    // Extract owner task ID from event details like "blocked (resource held by T1)"
    std::string extractOwnerFromDetails(const std::string& details) {
        std::string marker = "held by ";
        size_t pos = details.find(marker);
        if (pos != std::string::npos) {
            std::string owner = details.substr(pos + marker.length());
            // Remove trailing ) if present
            if (!owner.empty() && owner.back() == ')') {
                owner.pop_back();
            }
            return owner;
        }
        return "";
    }
    
    // Find which task was running at a specific time point
    std::string findRunningTaskAtTime(const std::vector<SimEvent>& events, int time) {
        std::string running = "";
        for (const auto& evt : events) {
            if (evt.time > time) break;
            if (evt.type == EventType::TASK_START || 
                evt.type == EventType::TASK_RESUME) {
                running = evt.task_id;
            }
            else if (evt.type == EventType::TASK_PREEMPTED || 
                     evt.type == EventType::TASK_BLOCKED ||
                     evt.type == EventType::TASK_COMPLETED) {
                if (evt.task_id == running) {
                    running = "";
                }
            }
            else if (evt.type == EventType::CONTEXT_SWITCH) {
                running = evt.task_id;
            }
        }
        return running;
    }
    
    // Build a human-readable causal chain
    std::string buildCausalChain(const InversionRecord& inv) {
        std::stringstream ss;
        ss << "Task " << inv.low_task_id << " holds resource " << inv.resource_id;
        ss << " -> Task " << inv.high_task_id << " requests " << inv.resource_id 
           << " and blocks at time " << inv.start_time;
        
        if (!inv.interfering_tasks.empty()) {
            ss << " -> ";
            for (size_t i = 0; i < inv.interfering_tasks.size(); i++) {
                if (i > 0) ss << ", ";
                ss << "Task " << inv.interfering_tasks[i];
            }
            ss << " preempt(s) " << inv.low_task_id 
               << " (intermediate priority interference)";
            ss << " -> " << inv.low_task_id << " cannot release " << inv.resource_id 
               << " promptly";
            ss << " -> " << inv.high_task_id << " experiences " 
               << inv.interference_time << " units of additional delay";
        }
        
        ss << " -> Blocking ends at time " << inv.end_time 
           << " (total=" << (inv.end_time - inv.start_time) << " units)";
        
        if (inv.caused_deadline_miss) {
            ss << " -> DEADLINE MISS for " << inv.high_task_id;
        }
        
        return ss.str();
    }
};

} // namespace pg

#endif // PRIORITYGUARD_DETECTOR_H

