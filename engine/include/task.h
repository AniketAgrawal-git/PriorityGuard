/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * task.h - Runtime task representation
 *
 * Manages task state during simulation: tracks execution progress,
 * effective priority (separate from base priority for inheritance),
 * critical section progress, and timing statistics.
 */

#ifndef PRIORITYGUARD_TASK_H
#define PRIORITYGUARD_TASK_H

#include "types.h"
#include <string>
#include <vector>
#include <algorithm>

namespace pg {

class Task {
public:
    // Configuration (immutable during simulation)
    std::string id;
    std::string name;
    int base_priority;
    int release_time;
    int total_execution;     // Total CPU time needed
    int relative_deadline;
    int absolute_deadline;   // release_time + relative_deadline
    std::vector<CriticalSection> critical_sections;

    // Runtime state (mutable during simulation)
    TaskState state;
    int effective_priority;  // May differ from base_priority under PIP/PCP
    int executed_time;       // CPU time consumed so far
    int remaining_time;      // total_execution - executed_time
    
    // Timing statistics
    int first_start_time;    // When task first started executing (-1 if never)
    int completion_time;     // When task completed (-1 if not completed)
    int blocked_time;        // Total time spent blocked
    int waiting_time;        // Total time spent in ready queue
    int block_start_time;    // When current blocking began (-1 if not blocked)
    int max_block_duration;  // Longest single blocking interval
    
    // Resource tracking
    std::string waiting_for_resource;  // Resource this task is blocked on ("" if none)
    std::vector<std::string> held_resources;  // Resources currently held by this task
    
    // For tracking which CS we're currently in or approaching
    int current_cs_index;    // Index into critical_sections (-1 = not in any CS)
    bool in_critical_section; // Whether currently inside a CS

    Task() : base_priority(0), release_time(0), total_execution(0),
             relative_deadline(0), absolute_deadline(0),
             state(TaskState::NOT_RELEASED), effective_priority(0),
             executed_time(0), remaining_time(0),
             first_start_time(-1), completion_time(-1),
             blocked_time(0), waiting_time(0), block_start_time(-1),
             max_block_duration(0), current_cs_index(-1),
             in_critical_section(false) {}

    // Initialize from configuration
    void initFromConfig(const TaskConfig& config) {
        id = config.id;
        name = config.name;
        base_priority = config.priority;
        effective_priority = config.priority;
        release_time = config.release_time;
        total_execution = config.execution_time;
        relative_deadline = config.deadline;
        absolute_deadline = release_time + config.deadline;
        critical_sections = config.critical_sections;
        remaining_time = total_execution;
        state = TaskState::NOT_RELEASED;
        executed_time = 0;
        first_start_time = -1;
        completion_time = -1;
        blocked_time = 0;
        waiting_time = 0;
        block_start_time = -1;
        max_block_duration = 0;
        current_cs_index = -1;
        in_critical_section = false;
        waiting_for_resource = "";
        held_resources.clear();
    }
    
    // Check if the task needs a resource at its current execution point
    // Returns resource_id if a CS starts at the current executed_time, "" otherwise
    std::string getResourceNeededNow() const {
        for (size_t i = 0; i < critical_sections.size(); i++) {
            const auto& cs = critical_sections[i];
            if (cs.offset == executed_time && !in_critical_section) {
                return cs.resource_id;
            }
        }
        return "";
    }
    
    // Get the critical section that starts at the current execution point
    // Returns index or -1 if none
    int getCSIndexAtCurrentPoint() const {
        for (size_t i = 0; i < critical_sections.size(); i++) {
            if (critical_sections[i].offset == executed_time) {
                return (int)i;
            }
        }
        return -1;
    }
    
    // Check if the task is about to exit its current critical section
    bool isCSEndingNow() const {
        if (!in_critical_section || current_cs_index < 0) return false;
        const auto& cs = critical_sections[current_cs_index];
        return (executed_time >= cs.offset + cs.duration);
    }
    
    // Get the resource being held in the current CS
    std::string getCurrentCSResource() const {
        if (current_cs_index >= 0 && current_cs_index < (int)critical_sections.size()) {
            return critical_sections[current_cs_index].resource_id;
        }
        return "";
    }
    
    // Record blocking start
    void startBlocking(int time, const std::string& resource) {
        state = TaskState::BLOCKED;
        block_start_time = time;
        waiting_for_resource = resource;
    }
    
    // Record blocking end and update statistics
    void endBlocking(int time) {
        if (block_start_time >= 0) {
            int duration = time - block_start_time;
            blocked_time += duration;
            if (duration > max_block_duration) {
                max_block_duration = duration;
            }
        }
        block_start_time = -1;
        waiting_for_resource = "";
        state = TaskState::READY;
    }
    
    // Check deadline
    bool hasDeadlineMissed(int current_time) const {
        if (state == TaskState::COMPLETED) {
            return completion_time > absolute_deadline;
        }
        // If not completed and past deadline
        return current_time > absolute_deadline && state != TaskState::COMPLETED;
    }

    // Add a held resource
    void acquireResource(const std::string& res) {
        held_resources.push_back(res);
    }
    
    // Release a held resource
    void releaseResource(const std::string& res) {
        held_resources.erase(
            std::remove(held_resources.begin(), held_resources.end(), res),
            held_resources.end()
        );
    }
    
    // Check if task holds a specific resource
    bool holdsResource(const std::string& res) const {
        for (const auto& r : held_resources) {
            if (r == res) return true;
        }
        return false;
    }
    
    // Generate TaskResult for output
    TaskResult toResult() const {
        TaskResult r;
        r.task_id = id;
        r.task_name = name;
        r.base_priority = base_priority;
        r.release_time = release_time;
        r.deadline = absolute_deadline;
        r.start_time = first_start_time;
        r.completion_time = completion_time;
        r.response_time = (completion_time >= 0) ? (completion_time - release_time) : -1;
        r.waiting_time = waiting_time;
        r.blocked_time = blocked_time;
        r.deadline_missed = (completion_time >= 0) ? (completion_time > absolute_deadline) : false;
        r.max_blocking_duration = max_block_duration;
        r.priority_inversions = 0; // Set by detector
        return r;
    }
};

} // namespace pg

#endif // PRIORITYGUARD_TASK_H

