/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * types.h - Core type definitions
 *
 * Defines enumerations and basic structures used throughout the engine:
 * task states, scheduling protocols, event types, and configuration.
 */

#ifndef PRIORITYGUARD_TYPES_H
#define PRIORITYGUARD_TYPES_H

#include <string>
#include <vector>

namespace pg {

// ============================================================================
// Task States
// ============================================================================
enum class TaskState {
    NOT_RELEASED,  // Task has not yet arrived (before release time)
    READY,         // Task is ready to execute (in ready queue)
    RUNNING,       // Task is currently executing on the CPU
    BLOCKED,       // Task is waiting for a mutex held by another task
    COMPLETED      // Task has finished all execution
};

inline std::string taskStateToString(TaskState s) {
    switch (s) {
        case TaskState::NOT_RELEASED: return "NOT_RELEASED";
        case TaskState::READY:        return "READY";
        case TaskState::RUNNING:      return "RUNNING";
        case TaskState::BLOCKED:      return "BLOCKED";
        case TaskState::COMPLETED:    return "COMPLETED";
        default:                      return "UNKNOWN";
    }
}

// ============================================================================
// Resource-Sharing Protocol
// ============================================================================
enum class Protocol {
    NONE,  // No priority inheritance - baseline scheduling with mutex blocking
    PIP,   // Priority Inheritance Protocol
    PCP    // Priority Ceiling Protocol
};

inline std::string protocolToString(Protocol p) {
    switch (p) {
        case Protocol::NONE: return "NONE";
        case Protocol::PIP:  return "PIP";
        case Protocol::PCP:  return "PCP";
        default:             return "UNKNOWN";
    }
}

inline Protocol protocolFromString(const std::string& s) {
    if (s == "PIP") return Protocol::PIP;
    if (s == "PCP") return Protocol::PCP;
    return Protocol::NONE;
}

// ============================================================================
// Event Types (for the chronological event log)
// ============================================================================
enum class EventType {
    TASK_RELEASE,           // Task arrives / becomes ready
    TASK_START,             // Task begins executing on CPU
    TASK_PREEMPTED,         // Task is preempted by higher-priority task
    TASK_RESUME,            // Task resumes execution after preemption
    TASK_COMPLETED,         // Task finishes all execution
    MUTEX_REQUEST,          // Task requests a mutex
    MUTEX_ACQUIRE,          // Task successfully acquires a mutex
    MUTEX_RELEASE,          // Task releases a mutex
    TASK_BLOCKED,           // Task blocks waiting for a mutex
    TASK_UNBLOCKED,         // Task wakes up after mutex is released
    PRIORITY_INHERIT,       // Priority inheritance triggered
    PRIORITY_RESTORE,       // Priority restored after inheritance
    DEADLINE_MISS,          // Task misses its deadline
    PCP_CEILING_BLOCK,      // PCP: task blocked by ceiling protocol
    CONTEXT_SWITCH,         // CPU context switch occurred
    SIMULATION_END          // Simulation terminated
};

inline std::string eventTypeToString(EventType e) {
    switch (e) {
        case EventType::TASK_RELEASE:       return "TASK_RELEASE";
        case EventType::TASK_START:         return "TASK_START";
        case EventType::TASK_PREEMPTED:     return "TASK_PREEMPTED";
        case EventType::TASK_RESUME:        return "TASK_RESUME";
        case EventType::TASK_COMPLETED:     return "TASK_COMPLETED";
        case EventType::MUTEX_REQUEST:      return "MUTEX_REQUEST";
        case EventType::MUTEX_ACQUIRE:      return "MUTEX_ACQUIRE";
        case EventType::MUTEX_RELEASE:      return "MUTEX_RELEASE";
        case EventType::TASK_BLOCKED:       return "TASK_BLOCKED";
        case EventType::TASK_UNBLOCKED:     return "TASK_UNBLOCKED";
        case EventType::PRIORITY_INHERIT:   return "PRIORITY_INHERIT";
        case EventType::PRIORITY_RESTORE:   return "PRIORITY_RESTORE";
        case EventType::DEADLINE_MISS:      return "DEADLINE_MISS";
        case EventType::PCP_CEILING_BLOCK:  return "PCP_CEILING_BLOCK";
        case EventType::CONTEXT_SWITCH:     return "CONTEXT_SWITCH";
        case EventType::SIMULATION_END:     return "SIMULATION_END";
        default:                            return "UNKNOWN";
    }
}

// ============================================================================
// Critical Section Definition
// ============================================================================
// Describes one critical section within a task's execution profile.
// A task's execution is a sequence of: [non-CS work] [CS] [non-CS work] ...
struct CriticalSection {
    std::string resource_id;     // ID of the shared resource / mutex required
    int duration;                // Execution time inside the critical section
    int offset;                  // Time offset from task start to CS start
                                 // (includes preceding non-CS segments)
};

// ============================================================================
// Task Configuration (input)
// ============================================================================
struct TaskConfig {
    std::string id;              // Unique task identifier
    std::string name;            // Human-readable name
    int priority;                // Base priority (higher number = higher priority)
    int release_time;            // Arrival / release time
    int execution_time;          // Total CPU execution demand (all segments)
    int deadline;                // Relative deadline (absolute = release + deadline)
    std::vector<CriticalSection> critical_sections;
};

// ============================================================================
// Resource Configuration (input)
// ============================================================================
struct ResourceConfig {
    std::string id;              // Unique resource identifier
    std::string name;            // Human-readable name
    // ceiling is computed at runtime from tasks that use it
};

// ============================================================================
// Simulation Event (output log entry)
// ============================================================================
struct SimEvent {
    int time;                    // Simulation time of the event
    EventType type;              // Type of event
    std::string task_id;         // Task involved (may be empty for some events)
    std::string resource_id;     // Resource involved (may be empty)
    std::string details;         // Human-readable description
    int base_priority;           // Task's base priority at event time
    int effective_priority;      // Task's effective priority at event time
};

// ============================================================================
// Task Result (per-task output metrics)
// ============================================================================
struct TaskResult {
    std::string task_id;
    std::string task_name;
    int base_priority;
    int release_time;
    int deadline;                // Absolute deadline
    int start_time;              // First time the task started executing
    int completion_time;         // Time task finished (or -1 if not completed)
    int response_time;           // completion_time - release_time
    int waiting_time;            // Time spent in READY state
    int blocked_time;            // Time spent in BLOCKED state
    bool deadline_missed;        // Whether absolute deadline was exceeded
    int max_blocking_duration;   // Longest single blocking interval
    int priority_inversions;     // Number of inversion episodes involving this task
};

// ============================================================================
// Priority Inversion Record
// ============================================================================
struct InversionRecord {
    int start_time;              // When blocking began
    int end_time;                // When blocking ended
    int direct_blocking_time;    // Time blocked purely due to resource holding
    int interference_time;       // Additional delay from intermediate tasks
    std::string high_task_id;    // High-priority task that was blocked
    std::string low_task_id;     // Low-priority task holding the resource
    std::string resource_id;     // Resource causing the block
    std::vector<std::string> interfering_tasks;  // Mid-priority tasks that preempted
    bool caused_deadline_miss;   // Whether this inversion led to a deadline miss
    std::string causal_chain;    // Human-readable explanation
};

// ============================================================================
// Overall Simulation Results
// ============================================================================
struct SimulationResults {
    Protocol protocol;
    int total_time;              // Total simulation duration
    int context_switches;
    int priority_inheritance_events;
    int protocol_blocking_events;
    double cpu_utilization;      // Fraction of time CPU was busy
    
    // Aggregate metrics
    double avg_response_time;
    int worst_response_time;
    double avg_waiting_time;
    int total_blocked_time;
    int max_blocking_duration;
    int total_priority_inversion_time;
    int deadline_misses;
    int completed_tasks;
    int total_tasks;
    
    std::vector<TaskResult> task_results;
    std::vector<InversionRecord> inversions;
    std::vector<SimEvent> events;
};

// ============================================================================
// Workload Configuration (full input)
// ============================================================================
struct WorkloadConfig {
    std::string name;
    std::string description;
    std::vector<TaskConfig> tasks;
    std::vector<ResourceConfig> resources;
    Protocol protocol;
    int simulation_limit;        // Maximum simulation time (safety bound)
};

// ============================================================================
// Recommendation
// ============================================================================
struct Recommendation {
    std::string problem;         // Detected issue
    std::string evidence;        // Supporting data from metrics
    std::string suggestion;      // Recommended change
    std::string benefit;         // Expected improvement
    std::string tradeoff;        // Possible downsides
};

} // namespace pg

#endif // PRIORITYGUARD_TYPES_H

