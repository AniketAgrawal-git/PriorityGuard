/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * mutex_manager.h - Mutex and resource management
 *
 * Manages mutex ownership, waiting queues, priority ceilings (for PCP),
 * and resource-related state transitions during simulation.
 */

#ifndef PRIORITYGUARD_MUTEX_MANAGER_H
#define PRIORITYGUARD_MUTEX_MANAGER_H

#include "types.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace pg {

// Represents a single mutex/shared resource at runtime
struct Mutex {
    std::string id;
    std::string name;
    std::string owner_task_id;       // Task currently holding the mutex ("" if free)
    std::vector<std::string> wait_queue;  // Tasks waiting for this mutex (by task_id)
    int ceiling;                     // Priority ceiling (for PCP), set at initialization
    bool locked;

    Mutex() : ceiling(0), locked(false) {}
};

class MutexManager {
public:
    std::map<std::string, Mutex> mutexes;

    // Initialize mutexes from resource configs
    void init(const std::vector<ResourceConfig>& resources) {
        mutexes.clear();
        for (const auto& rc : resources) {
            Mutex m;
            m.id = rc.id;
            m.name = rc.name;
            m.owner_task_id = "";
            m.locked = false;
            m.ceiling = 0;
            mutexes[rc.id] = m;
        }
    }

    // Compute priority ceilings for PCP based on which tasks use each resource
    void computeCeilings(const std::vector<TaskConfig>& tasks) {
        // Reset all ceilings
        for (auto& pair : mutexes) {
            pair.second.ceiling = 0;
        }
        // For each task, update ceiling of each resource it uses
        for (const auto& task : tasks) {
            for (const auto& cs : task.critical_sections) {
                auto it = mutexes.find(cs.resource_id);
                if (it != mutexes.end()) {
                    if (task.priority > it->second.ceiling) {
                        it->second.ceiling = task.priority;
                    }
                }
            }
        }
    }

    // Check if a mutex exists
    bool exists(const std::string& resource_id) const {
        return mutexes.find(resource_id) != mutexes.end();
    }

    // Check if a mutex is free
    bool isFree(const std::string& resource_id) const {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return false;
        return !it->second.locked;
    }

    // Get the owner of a mutex
    std::string getOwner(const std::string& resource_id) const {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return "";
        return it->second.owner_task_id;
    }

    // Acquire a mutex (caller must check it's free first)
    bool acquire(const std::string& resource_id, const std::string& task_id) {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return false;
        if (it->second.locked) return false;
        it->second.locked = true;
        it->second.owner_task_id = task_id;
        return true;
    }

    // Release a mutex (returns list of waiting tasks to potentially unblock)
    std::vector<std::string> release(const std::string& resource_id,
                                      const std::string& task_id) {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return {};
        if (it->second.owner_task_id != task_id) return {};
        
        it->second.locked = false;
        it->second.owner_task_id = "";
        
        // Return the wait queue for the scheduler to handle
        std::vector<std::string> waiters = it->second.wait_queue;
        return waiters;
    }

    // Add a task to the wait queue of a mutex
    void addWaiter(const std::string& resource_id, const std::string& task_id) {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return;
        // Avoid duplicates
        for (const auto& w : it->second.wait_queue) {
            if (w == task_id) return;
        }
        it->second.wait_queue.push_back(task_id);
    }

    // Remove a task from the wait queue
    void removeWaiter(const std::string& resource_id, const std::string& task_id) {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return;
        auto& wq = it->second.wait_queue;
        wq.erase(std::remove(wq.begin(), wq.end(), task_id), wq.end());
    }

    // Get ceiling of a resource (for PCP)
    int getCeiling(const std::string& resource_id) const {
        auto it = mutexes.find(resource_id);
        if (it == mutexes.end()) return 0;
        return it->second.ceiling;
    }

    // Get the current system ceiling (max ceiling of all currently locked mutexes)
    // Used by PCP to decide if a task can acquire a new resource
    int getSystemCeiling(const std::string& exclude_task_id = "") const {
        int max_ceil = 0;
        for (const auto& pair : mutexes) {
            if (pair.second.locked && pair.second.owner_task_id != exclude_task_id) {
                if (pair.second.ceiling > max_ceil) {
                    max_ceil = pair.second.ceiling;
                }
            }
        }
        return max_ceil;
    }

    // Get all mutexes held by a task
    std::vector<std::string> getHeldBy(const std::string& task_id) const {
        std::vector<std::string> result;
        for (const auto& pair : mutexes) {
            if (pair.second.locked && pair.second.owner_task_id == task_id) {
                result.push_back(pair.first);
            }
        }
        return result;
    }

    // Get all mutex states for reporting
    std::map<std::string, Mutex> getState() const {
        return mutexes;
    }

    // Reset all mutexes
    void reset() {
        for (auto& pair : mutexes) {
            pair.second.locked = false;
            pair.second.owner_task_id = "";
            pair.second.wait_queue.clear();
        }
    }
};

} // namespace pg

#endif // PRIORITYGUARD_MUTEX_MANAGER_H

