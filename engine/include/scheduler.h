/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * scheduler.h - Core discrete-event simulation scheduler
 *
 * Implements fixed-priority preemptive scheduling with support for:
 * - No protocol (baseline with mutex blocking)
 * - Priority Inheritance Protocol (PIP)
 * - Priority Ceiling Protocol (PCP)
 *
 * The scheduler operates in discrete time steps, maintaining task states,
 * handling resource requests/releases, and producing a chronological event log.
 */

#ifndef PRIORITYGUARD_SCHEDULER_H
#define PRIORITYGUARD_SCHEDULER_H

#include "types.h"
#include "task.h"
#include "mutex_manager.h"
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <sstream>

namespace pg {

class Scheduler {
public:
    // Configuration
    Protocol protocol;
    int simulation_limit;

    // State
    std::vector<Task> tasks;
    MutexManager mutex_mgr;
    int current_time;
    std::string running_task_id;  // Currently running task ("" if CPU idle)
    
    // Output
    std::vector<SimEvent> event_log;
    int context_switch_count;
    int priority_inheritance_count;
    int protocol_blocking_count;

    Scheduler() : protocol(Protocol::NONE), simulation_limit(1000),
                  current_time(0), context_switch_count(0),
                  priority_inheritance_count(0), protocol_blocking_count(0) {}

    // ========================================================================
    // Initialization
    // ========================================================================
    
    void init(const WorkloadConfig& config) {
        protocol = config.protocol;
        simulation_limit = config.simulation_limit;
        current_time = 0;
        running_task_id = "";
        context_switch_count = 0;
        priority_inheritance_count = 0;
        protocol_blocking_count = 0;
        event_log.clear();
        
        // Initialize tasks
        tasks.clear();
        for (const auto& tc : config.tasks) {
            Task t;
            t.initFromConfig(tc);
            tasks.push_back(t);
        }
        
        // Initialize mutexes
        mutex_mgr.init(config.resources);
        
        // Compute ceilings if PCP
        if (protocol == Protocol::PCP) {
            mutex_mgr.computeCeilings(config.tasks);
        }
    }

    // ========================================================================
    // Main simulation loop
    // ========================================================================
    
    SimulationResults run() {
        while (current_time <= simulation_limit) {
            bool any_active = false;
            
            // 1. Release tasks that arrive at current_time
            releaseArrivingTasks();
            
            // 2. Check for critical section exits in running task
            if (!running_task_id.empty()) {
                Task* running = getTask(running_task_id);
                if (running && running->isCSEndingNow()) {
                    handleCSExit(*running);
                }
            }
            
            // 3. Schedule: pick highest effective-priority ready task
            scheduleNext();
            
            // 4. Check for critical section entries in the (possibly new) running task
            //    This must happen AFTER scheduling so newly started tasks can
            //    attempt resource acquisition before their first time unit executes.
            if (!running_task_id.empty()) {
                Task* running = getTask(running_task_id);
                if (running && running->state == TaskState::RUNNING) {
                    std::string needed = running->getResourceNeededNow();
                    if (!needed.empty()) {
                        handleCSEntry(*running, needed);
                        // If the task got blocked, re-schedule
                        if (running->state == TaskState::BLOCKED) {
                            scheduleNext();
                        }
                    }
                }
            }
            
            // 5. Execute one time unit for the running task
            if (!running_task_id.empty()) {
                Task* running = getTask(running_task_id);
                if (running && running->state == TaskState::RUNNING) {
                    running->executed_time++;
                    running->remaining_time--;
                    any_active = true;
                    
                    // After execution, check if a CS just started (for non-zero offsets)
                    // and if so, try to acquire the resource
                    std::string needed_after = running->getResourceNeededNow();
                    if (!needed_after.empty()) {
                        handleCSEntry(*running, needed_after);
                        if (running->state == TaskState::BLOCKED) {
                            scheduleNext();
                        }
                    }
                    
                    // Check for CS exit after execution
                    if (running->state == TaskState::RUNNING && running->isCSEndingNow()) {
                        handleCSExit(*running, current_time + 1);
                    }
                    
                    // Check if task completed
                    if (running->state == TaskState::RUNNING && running->remaining_time <= 0) {
                        handleTaskCompletion(*running, current_time + 1);
                    }
                }
            }
            
            // 6. Update waiting time for ready tasks (not running)
            for (auto& t : tasks) {
                if (t.state == TaskState::READY && t.id != running_task_id) {
                    t.waiting_time++;
                }
            }
            
            // 7. Check deadlines
            checkDeadlines();
            
            // Check if all tasks are completed or no tasks remain
            bool all_done = true;
            for (const auto& t : tasks) {
                if (t.state != TaskState::COMPLETED) {
                    all_done = false;
                    if (t.state != TaskState::NOT_RELEASED || t.release_time > current_time) {
                        any_active = true;
                    }
                }
            }
            
            current_time++;
            
            if (all_done) break;
            if (!any_active && allTasksReleasedOrDone()) break;
        }
        
        logEvent(EventType::SIMULATION_END, "", "", "Simulation completed at time " + std::to_string(current_time));
        return buildResults();
    }

    // ========================================================================
    // Step-by-step simulation (for interactive mode)
    // ========================================================================
    
    // Initialize for stepping
    void initForStepping(const WorkloadConfig& config) {
        init(config);
    }
    
    // Execute one time step, returns true if simulation should continue
    bool step() {
        if (current_time > simulation_limit) return false;
        
        // Check completion
        bool all_done = true;
        for (const auto& t : tasks) {
            if (t.state != TaskState::COMPLETED) {
                all_done = false;
                break;
            }
        }
        if (all_done) return false;
        
        // Same logic as run() but one iteration
        releaseArrivingTasks();
        
        if (!running_task_id.empty()) {
            Task* running = getTask(running_task_id);
            if (running && running->isCSEndingNow()) {
                handleCSExit(*running);
            }
        }
        
        scheduleNext();
        
        // Check CS entry for running task (after scheduling)
        if (!running_task_id.empty()) {
            Task* running = getTask(running_task_id);
            if (running && running->state == TaskState::RUNNING) {
                std::string needed = running->getResourceNeededNow();
                if (!needed.empty()) {
                    handleCSEntry(*running, needed);
                    if (running->state == TaskState::BLOCKED) {
                        scheduleNext();
                    }
                }
            }
        }
        
        if (!running_task_id.empty()) {
            Task* running = getTask(running_task_id);
            if (running && running->state == TaskState::RUNNING) {
                running->executed_time++;
                running->remaining_time--;
                
                // Check CS entry after execution (non-zero offset CSes)
                std::string needed_after = running->getResourceNeededNow();
                if (!needed_after.empty()) {
                    handleCSEntry(*running, needed_after);
                    if (running->state == TaskState::BLOCKED) {
                        scheduleNext();
                    }
                }
                
                // Check CS exit
                if (running->state == TaskState::RUNNING && running->isCSEndingNow()) {
                    handleCSExit(*running, current_time + 1);
                }
                
                if (running->state == TaskState::RUNNING && running->remaining_time <= 0) {
                    handleTaskCompletion(*running, current_time + 1);
                }
            }
        }
        
        for (auto& t : tasks) {
            if (t.state == TaskState::READY && t.id != running_task_id) {
                t.waiting_time++;
            }
        }
        
        checkDeadlines();
        current_time++;
        
        // Check if done
        for (const auto& t : tasks) {
            if (t.state != TaskState::COMPLETED) return true;
        }
        return false;
    }

private:
    // ========================================================================
    // Task lookup
    // ========================================================================
    
    Task* getTask(const std::string& id) {
        for (auto& t : tasks) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
    
    const Task* getTaskConst(const std::string& id) const {
        for (const auto& t : tasks) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    // ========================================================================
    // Event logging
    // ========================================================================
    
    void logEvent(EventType type, const std::string& task_id,
                  const std::string& resource_id, const std::string& details,
                  int event_time = -1) {
        SimEvent e;
        e.time = (event_time >= 0) ? event_time : current_time;
        e.type = type;
        e.task_id = task_id;
        e.resource_id = resource_id;
        e.details = details;
        
        const Task* t = getTaskConst(task_id);
        if (t) {
            e.base_priority = t->base_priority;
            e.effective_priority = t->effective_priority;
        } else {
            e.base_priority = 0;
            e.effective_priority = 0;
        }
        
        event_log.push_back(e);
    }

    // ========================================================================
    // Task release
    // ========================================================================
    
    void releaseArrivingTasks() {
        for (auto& t : tasks) {
            if (t.state == TaskState::NOT_RELEASED && t.release_time == current_time) {
                t.state = TaskState::READY;
                logEvent(EventType::TASK_RELEASE, t.id, "",
                        "Task " + t.name + " released (priority=" + 
                        std::to_string(t.base_priority) + ")");
            }
        }
    }

    // ========================================================================
    // Scheduling decision
    // ========================================================================
    
    void scheduleNext() {
        // Find highest effective-priority ready task
        Task* best = nullptr;
        for (auto& t : tasks) {
            if (t.state == TaskState::READY || 
                (t.state == TaskState::RUNNING && t.id == running_task_id)) {
                if (!best || t.effective_priority > best->effective_priority ||
                    (t.effective_priority == best->effective_priority && 
                     t.release_time < best->release_time) ||
                    (t.effective_priority == best->effective_priority &&
                     t.release_time == best->release_time && t.id < best->id)) {
                    best = &t;
                }
            }
        }
        
        if (!best) {
            // CPU idle
            if (!running_task_id.empty()) {
                running_task_id = "";
            }
            return;
        }
        
        std::string new_runner = best->id;
        
        if (new_runner != running_task_id) {
            // Context switch needed
            if (!running_task_id.empty()) {
                Task* old = getTask(running_task_id);
                if (old && old->state == TaskState::RUNNING) {
                    old->state = TaskState::READY;
                    logEvent(EventType::TASK_PREEMPTED, old->id, "",
                            "Task " + old->name + " preempted by " + best->name);
                }
            }
            
            if (best->state == TaskState::READY) {
                best->state = TaskState::RUNNING;
                if (best->first_start_time < 0) {
                    best->first_start_time = current_time;
                    logEvent(EventType::TASK_START, best->id, "",
                            "Task " + best->name + " starts executing");
                } else {
                    logEvent(EventType::TASK_RESUME, best->id, "",
                            "Task " + best->name + " resumes executing");
                }
            }
            
            if (!running_task_id.empty() && running_task_id != new_runner) {
                context_switch_count++;
                logEvent(EventType::CONTEXT_SWITCH, new_runner, "",
                        "Context switch to " + best->name);
            }
            
            running_task_id = new_runner;
        }
    }

    // ========================================================================
    // Critical Section Entry
    // ========================================================================
    
    void handleCSEntry(Task& task, const std::string& resource_id) {
        logEvent(EventType::MUTEX_REQUEST, task.id, resource_id,
                "Task " + task.name + " requests resource " + resource_id);
        
        if (protocol == Protocol::PCP) {
            // PCP: Check system ceiling
            int sys_ceil = mutex_mgr.getSystemCeiling(task.id);
            if (mutex_mgr.isFree(resource_id) && 
                (task.effective_priority > sys_ceil || sys_ceil == 0)) {
                // Can acquire
                acquireResource(task, resource_id);
            } else if (mutex_mgr.isFree(resource_id) && 
                       task.effective_priority <= sys_ceil) {
                // PCP ceiling block: resource is free but system ceiling blocks
                protocol_blocking_count++;
                logEvent(EventType::PCP_CEILING_BLOCK, task.id, resource_id,
                        "Task " + task.name + " blocked by PCP ceiling (sys_ceil=" +
                        std::to_string(sys_ceil) + ")");
                blockTask(task, resource_id);
                
                // Under PCP, the task holding the ceiling-raising mutex inherits
                // the blocked task's priority
                boostCeilingOwner(task, sys_ceil);
            } else {
                // Resource is held by another task
                std::string owner_id = mutex_mgr.getOwner(resource_id);
                logEvent(EventType::TASK_BLOCKED, task.id, resource_id,
                        "Task " + task.name + " blocked (resource held by " + owner_id + ")");
                blockTask(task, resource_id);
                
                // PCP also does priority inheritance
                if (!owner_id.empty()) {
                    applyPriorityInheritance(task, owner_id);
                }
            }
        } else if (protocol == Protocol::PIP) {
            // PIP: Direct acquisition or blocking with inheritance
            if (mutex_mgr.isFree(resource_id)) {
                acquireResource(task, resource_id);
            } else {
                std::string owner_id = mutex_mgr.getOwner(resource_id);
                logEvent(EventType::TASK_BLOCKED, task.id, resource_id,
                        "Task " + task.name + " blocked (resource held by " + owner_id + ")");
                blockTask(task, resource_id);
                
                // Apply priority inheritance
                if (!owner_id.empty()) {
                    applyPriorityInheritance(task, owner_id);
                }
            }
        } else {
            // No protocol: simple blocking
            if (mutex_mgr.isFree(resource_id)) {
                acquireResource(task, resource_id);
            } else {
                std::string owner_id = mutex_mgr.getOwner(resource_id);
                logEvent(EventType::TASK_BLOCKED, task.id, resource_id,
                        "Task " + task.name + " blocked (resource held by " + owner_id + ")");
                blockTask(task, resource_id);
            }
        }
    }

    void acquireResource(Task& task, const std::string& resource_id, int event_time = -1) {
        int t = (event_time >= 0) ? event_time : current_time;
        mutex_mgr.acquire(resource_id, task.id);
        task.acquireResource(resource_id);
        int cs_idx = task.getCSIndexAtCurrentPoint();
        if (cs_idx >= 0) {
            task.current_cs_index = cs_idx;
            task.in_critical_section = true;
        }
        logEvent(EventType::MUTEX_ACQUIRE, task.id, resource_id,
                "Task " + task.name + " acquired resource " + resource_id, t);
    }

    void blockTask(Task& task, const std::string& resource_id) {
        task.startBlocking(current_time, resource_id);
        mutex_mgr.addWaiter(resource_id, task.id);
        
        // The blocked task is no longer running
        if (running_task_id == task.id) {
            running_task_id = "";
        }
    }

    // ========================================================================
    // Priority Inheritance
    // ========================================================================
    
    void applyPriorityInheritance(const Task& blocked_task, const std::string& owner_id) {
        Task* owner = getTask(owner_id);
        if (!owner) return;
        
        if (blocked_task.effective_priority > owner->effective_priority) {
            int old_prio = owner->effective_priority;
            owner->effective_priority = blocked_task.effective_priority;
            priority_inheritance_count++;
            
            logEvent(EventType::PRIORITY_INHERIT, owner->id, "",
                    "Task " + owner->name + " inherits priority " +
                    std::to_string(blocked_task.effective_priority) +
                    " from " + blocked_task.name +
                    " (was " + std::to_string(old_prio) + ")");
            
            // Transitive inheritance: if owner is also blocked, propagate
            if (owner->state == TaskState::BLOCKED && !owner->waiting_for_resource.empty()) {
                std::string res_owner = mutex_mgr.getOwner(owner->waiting_for_resource);
                if (!res_owner.empty() && res_owner != owner->id) {
                    applyPriorityInheritance(*owner, res_owner);
                }
            }
        }
    }

    // For PCP: boost the owner of the mutex causing the ceiling block
    void boostCeilingOwner(const Task& blocked_task, int sys_ceil) {
        // Find which locked mutex causes the system ceiling
        for (auto& pair : mutex_mgr.mutexes) {
            if (pair.second.locked && pair.second.ceiling == sys_ceil) {
                std::string owner_id = pair.second.owner_task_id;
                if (!owner_id.empty()) {
                    applyPriorityInheritance(blocked_task, owner_id);
                }
                break;
            }
        }
    }

    // ========================================================================
    // Critical Section Exit
    // ========================================================================
    
    void handleCSExit(Task& task, int event_time = -1) {
        if (task.current_cs_index < 0) return;
        int t = (event_time >= 0) ? event_time : current_time;
        
        std::string resource_id = task.critical_sections[task.current_cs_index].resource_id;
        
        // Release the mutex
        std::vector<std::string> waiters = mutex_mgr.release(resource_id, task.id);
        task.releaseResource(resource_id);
        task.in_critical_section = false;
        task.current_cs_index = -1;
        
        logEvent(EventType::MUTEX_RELEASE, task.id, resource_id,
                "Task " + task.name + " released resource " + resource_id, t);
        
        // Restore effective priority
        recalculateEffectivePriority(task, t);
        
        // Unblock waiting tasks
        // Pick highest priority waiter to give the resource to
        Task* best_waiter = nullptr;
        for (const auto& wid : waiters) {
            Task* w = getTask(wid);
            if (w && w->state == TaskState::BLOCKED && w->waiting_for_resource == resource_id) {
                if (!best_waiter || w->effective_priority > best_waiter->effective_priority ||
                    (w->effective_priority == best_waiter->effective_priority &&
                     w->base_priority > best_waiter->base_priority)) {
                    best_waiter = w;
                }
            }
        }
        
        if (best_waiter) {
            // Unblock this task and give it the resource
            best_waiter->endBlocking(t);
            mutex_mgr.removeWaiter(resource_id, best_waiter->id);
            
            logEvent(EventType::TASK_UNBLOCKED, best_waiter->id, resource_id,
                    "Task " + best_waiter->name + " unblocked", t);
            
            // The task gets the resource
            acquireResource(*best_waiter, resource_id, t);
        }
    }

    // Recalculate effective priority after releasing a resource
    void recalculateEffectivePriority(Task& task, int event_time = -1) {
        int t = (event_time >= 0) ? event_time : current_time;
        int max_inherited = task.base_priority;
        
        if (protocol == Protocol::PIP || protocol == Protocol::PCP) {
            // Check if any tasks are still blocked on resources we hold
            for (const auto& res_id : task.held_resources) {
                auto it = mutex_mgr.mutexes.find(res_id);
                if (it != mutex_mgr.mutexes.end()) {
                    for (const auto& wid : it->second.wait_queue) {
                        const Task* waiter = getTaskConst(wid);
                        if (waiter && waiter->effective_priority > max_inherited) {
                            max_inherited = waiter->effective_priority;
                        }
                    }
                }
            }
        }
        
        if (task.effective_priority != max_inherited) {
            int old_prio = task.effective_priority;
            task.effective_priority = max_inherited;
            logEvent(EventType::PRIORITY_RESTORE, task.id, "",
                    "Task " + task.name + " priority restored to " +
                    std::to_string(max_inherited) +
                    " (was " + std::to_string(old_prio) + ")", t);
        }
    }

    // ========================================================================
    // Task Completion
    // ========================================================================
    
    void handleTaskCompletion(Task& task, int event_time = -1) {
        int t = (event_time >= 0) ? event_time : current_time;
        // Release any still-held resources (shouldn't happen in well-formed workloads)
        std::vector<std::string> held = task.held_resources;
        for (const auto& res : held) {
            std::vector<std::string> waiters = mutex_mgr.release(res, task.id);
            task.releaseResource(res);
            // Unblock waiters
            for (const auto& wid : waiters) {
                Task* w = getTask(wid);
                if (w && w->state == TaskState::BLOCKED && w->waiting_for_resource == res) {
                    w->endBlocking(t);
                    mutex_mgr.removeWaiter(res, w->id);
                    logEvent(EventType::TASK_UNBLOCKED, w->id, res,
                            "Task " + w->name + " unblocked (owner completed)", t);
                    acquireResource(*w, res, t);
                    break; // Only unblock highest priority waiter
                }
            }
        }
        
        task.state = TaskState::COMPLETED;
        task.completion_time = t;
        task.in_critical_section = false;
        task.current_cs_index = -1;
        task.effective_priority = task.base_priority;
        
        logEvent(EventType::TASK_COMPLETED, task.id, "",
                "Task " + task.name + " completed at time " + std::to_string(t), t);
        
        if (task.id == running_task_id) {
            running_task_id = "";
        }
    }

    // ========================================================================
    // Deadline checks
    // ========================================================================
    
    void checkDeadlines() {
        for (auto& t : tasks) {
            if (t.state != TaskState::COMPLETED && t.state != TaskState::NOT_RELEASED) {
                if (current_time >= t.absolute_deadline && t.completion_time < 0) {
                    // Only log once by checking if we already logged for this task
                    bool already_logged = false;
                    for (const auto& e : event_log) {
                        if (e.type == EventType::DEADLINE_MISS && e.task_id == t.id) {
                            already_logged = true;
                            break;
                        }
                    }
                    if (!already_logged) {
                        logEvent(EventType::DEADLINE_MISS, t.id, "",
                                "Task " + t.name + " MISSED deadline at time " +
                                std::to_string(t.absolute_deadline));
                    }
                }
            }
        }
    }

    // ========================================================================
    // Utility
    // ========================================================================
    
    bool allTasksReleasedOrDone() const {
        for (const auto& t : tasks) {
            if (t.state == TaskState::NOT_RELEASED && t.release_time > current_time) {
                return false;
            }
        }
        return true;
    }

    // ========================================================================
    // Build results
    // ========================================================================
    
    SimulationResults buildResults() {
        SimulationResults results;
        results.protocol = protocol;
        results.total_time = current_time;
        results.context_switches = context_switch_count;
        results.priority_inheritance_events = priority_inheritance_count;
        results.protocol_blocking_events = protocol_blocking_count;
        results.events = event_log;
        
        // Per-task results
        int completed = 0;
        int total_resp = 0;
        int worst_resp = 0;
        int total_wait = 0;
        int total_block = 0;
        int max_block = 0;
        int deadline_misses = 0;
        int busy_time = 0;
        
        for (const auto& t : tasks) {
            TaskResult tr = t.toResult();
            results.task_results.push_back(tr);
            
            if (t.state == TaskState::COMPLETED) {
                completed++;
                int resp = t.completion_time - t.release_time;
                total_resp += resp;
                if (resp > worst_resp) worst_resp = resp;
                if (t.completion_time > t.absolute_deadline) deadline_misses++;
            }
            total_wait += t.waiting_time;
            total_block += t.blocked_time;
            if (t.max_block_duration > max_block) max_block = t.max_block_duration;
            busy_time += t.executed_time;
        }
        
        results.completed_tasks = completed;
        results.total_tasks = (int)tasks.size();
        results.deadline_misses = deadline_misses;
        results.avg_response_time = completed > 0 ? (double)total_resp / completed : 0.0;
        results.worst_response_time = worst_resp;
        results.avg_waiting_time = tasks.size() > 0 ? (double)total_wait / tasks.size() : 0.0;
        results.total_blocked_time = total_block;
        results.max_blocking_duration = max_block;
        results.total_priority_inversion_time = 0; // Set by detector
        results.cpu_utilization = current_time > 0 ? (double)busy_time / current_time : 0.0;
        
        return results;
    }
};

} // namespace pg

#endif // PRIORITYGUARD_SCHEDULER_H
