/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * workload_generator.h - Configurable random workload generation
 *
 * Generates valid, reproducible task sets for batch experiments.
 * Uses a seeded random number generator for deterministic results.
 */

#ifndef PRIORITYGUARD_WORKLOAD_GENERATOR_H
#define PRIORITYGUARD_WORKLOAD_GENERATOR_H

#include "types.h"
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <sstream>

namespace pg {

struct GeneratorConfig {
    int num_tasks;                   // Number of tasks to generate
    int num_resources;               // Number of shared resources
    int min_priority;                // Minimum task priority
    int max_priority;                // Maximum task priority
    int min_execution;               // Minimum total execution time
    int max_execution;               // Maximum total execution time
    int min_cs_length;               // Minimum critical section length
    int max_cs_length;               // Maximum critical section length
    double resource_sharing_prob;    // Probability a task uses a shared resource [0,1]
    int max_release_time;            // Maximum release time
    double deadline_tightness;       // Multiplier for deadline (1.0 = tight, 3.0 = loose)
    int num_workloads;               // Number of task sets to generate
    unsigned int seed;               // Random seed for reproducibility
    
    GeneratorConfig() : num_tasks(5), num_resources(2),
                         min_priority(1), max_priority(10),
                         min_execution(3), max_execution(15),
                         min_cs_length(1), max_cs_length(5),
                         resource_sharing_prob(0.5),
                         max_release_time(10),
                         deadline_tightness(2.0),
                         num_workloads(10), seed(42) {}
};

class WorkloadGenerator {
public:
    // Generate multiple workload configurations
    std::vector<WorkloadConfig> generate(const GeneratorConfig& config) {
        std::vector<WorkloadConfig> workloads;
        
        for (int w = 0; w < config.num_workloads; w++) {
            // Each workload gets a different seed derived from base seed
            unsigned int workload_seed = config.seed + w;
            WorkloadConfig wc = generateOne(config, workload_seed, w);
            workloads.push_back(wc);
        }
        
        return workloads;
    }
    
    // Generate a single workload
    WorkloadConfig generateOne(const GeneratorConfig& config, unsigned int seed, int index) {
        std::mt19937 rng(seed);
        WorkloadConfig wc;
        
        std::stringstream ss;
        ss << "Generated_Workload_" << index;
        wc.name = ss.str();
        wc.description = "Auto-generated workload (seed=" + std::to_string(seed) + ")";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 500;
        
        // Generate resources
        for (int r = 0; r < config.num_resources; r++) {
            ResourceConfig rc;
            rc.id = "R" + std::to_string(r + 1);
            rc.name = "Resource_" + std::to_string(r + 1);
            wc.resources.push_back(rc);
        }
        
        // Generate unique priorities
        std::vector<int> priorities;
        std::uniform_int_distribution<int> prio_dist(config.min_priority, config.max_priority);
        for (int i = 0; i < config.num_tasks; i++) {
            int p;
            do {
                p = prio_dist(rng);
            } while (std::find(priorities.begin(), priorities.end(), p) != priorities.end() &&
                     priorities.size() < (size_t)(config.max_priority - config.min_priority + 1));
            priorities.push_back(p);
        }
        std::sort(priorities.begin(), priorities.end());
        
        // Generate tasks
        std::uniform_int_distribution<int> exec_dist(config.min_execution, config.max_execution);
        std::uniform_int_distribution<int> release_dist(0, config.max_release_time);
        std::uniform_int_distribution<int> cs_dist(config.min_cs_length, config.max_cs_length);
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        
        for (int i = 0; i < config.num_tasks; i++) {
            TaskConfig tc;
            tc.id = "T" + std::to_string(i + 1);
            tc.name = "Task_" + std::to_string(i + 1);
            tc.priority = priorities[i];
            tc.release_time = release_dist(rng);
            tc.execution_time = exec_dist(rng);
            
            // Generate critical sections
            if (prob_dist(rng) < config.resource_sharing_prob && config.num_resources > 0) {
                // Pick a random resource
                std::uniform_int_distribution<int> res_dist(0, config.num_resources - 1);
                int res_idx = res_dist(rng);
                
                int cs_len = cs_dist(rng);
                // Ensure CS fits within execution time
                if (cs_len >= tc.execution_time) {
                    cs_len = std::max(1, tc.execution_time - 2);
                }
                
                // Place CS at a valid offset
                int max_offset = tc.execution_time - cs_len;
                if (max_offset < 0) max_offset = 0;
                std::uniform_int_distribution<int> offset_dist(0, std::max(0, max_offset - 1));
                int offset = (max_offset > 0) ? offset_dist(rng) : 0;
                // Ensure offset is at least 0 but leaves room for CS
                if (offset + cs_len > tc.execution_time) {
                    offset = std::max(0, tc.execution_time - cs_len);
                }
                
                CriticalSection cs;
                cs.resource_id = wc.resources[res_idx].id;
                cs.duration = cs_len;
                cs.offset = offset;
                tc.critical_sections.push_back(cs);
            }
            
            // Set deadline based on tightness
            tc.deadline = (int)(tc.execution_time * config.deadline_tightness);
            if (tc.deadline < tc.execution_time) {
                tc.deadline = tc.execution_time + 2;
            }
            
            wc.tasks.push_back(tc);
        }
        
        return wc;
    }
    
    // Generate predefined classic priority inversion scenario
    static WorkloadConfig classicInversion() {
        WorkloadConfig wc;
        wc.name = "Classic Priority Inversion";
        wc.description = "Three tasks demonstrating the classic priority inversion scenario. "
                          "Low-priority Task L holds a mutex, high-priority Task H blocks on it, "
                          "and medium-priority Task M preempts L, delaying H.";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 50;
        
        // One shared resource
        ResourceConfig r1;
        r1.id = "R1";
        r1.name = "Shared_Mutex";
        wc.resources.push_back(r1);
        
        // Task L (Low priority): arrives first, acquires R1 early
        TaskConfig tl;
        tl.id = "TL";
        tl.name = "Task_L";
        tl.priority = 1;
        tl.release_time = 0;
        tl.execution_time = 10;
        tl.deadline = 30;
        CriticalSection cs_l;
        cs_l.resource_id = "R1";
        cs_l.duration = 6;   // Holds mutex for 6 time units
        cs_l.offset = 1;     // Acquires at execution time 1
        tl.critical_sections.push_back(cs_l);
        wc.tasks.push_back(tl);
        
        // Task M (Medium priority): arrives at time 2
        TaskConfig tm;
        tm.id = "TM";
        tm.name = "Task_M";
        tm.priority = 5;
        tm.release_time = 2;
        tm.execution_time = 5;
        tm.deadline = 20;
        // No critical sections - pure CPU work
        wc.tasks.push_back(tm);
        
        // Task H (High priority): arrives at time 3, needs R1
        TaskConfig th;
        th.id = "TH";
        th.name = "Task_H";
        th.priority = 10;
        th.release_time = 3;
        th.execution_time = 4;
        th.deadline = 15;
        CriticalSection cs_h;
        cs_h.resource_id = "R1";
        cs_h.duration = 2;   // Short CS
        cs_h.offset = 0;     // Needs mutex immediately
        th.critical_sections.push_back(cs_h);
        wc.tasks.push_back(th);
        
        return wc;
    }
    
    // No priority inversion scenario
    static WorkloadConfig noInversion() {
        WorkloadConfig wc;
        wc.name = "No Priority Inversion";
        wc.description = "Tasks that do not contend for shared resources in a way "
                          "that produces priority inversion.";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 30;
        
        ResourceConfig r1;
        r1.id = "R1";
        r1.name = "Resource_1";
        wc.resources.push_back(r1);
        
        // Task A (priority 5): uses R1 early
        TaskConfig ta;
        ta.id = "TA";
        ta.name = "Task_A";
        ta.priority = 5;
        ta.release_time = 0;
        ta.execution_time = 6;
        ta.deadline = 15;
        CriticalSection cs_a;
        cs_a.resource_id = "R1";
        cs_a.duration = 2;
        cs_a.offset = 1;
        ta.critical_sections.push_back(cs_a);
        wc.tasks.push_back(ta);
        
        // Task B (priority 3): arrives later, no resource contention
        TaskConfig tb;
        tb.id = "TB";
        tb.name = "Task_B";
        tb.priority = 3;
        tb.release_time = 8;
        tb.execution_time = 4;
        tb.deadline = 20;
        wc.tasks.push_back(tb);
        
        // Task C (priority 8): arrives after A finishes CS
        TaskConfig tc;
        tc.id = "TC";
        tc.name = "Task_C";
        tc.priority = 8;
        tc.release_time = 5;
        tc.execution_time = 3;
        tc.deadline = 12;
        wc.tasks.push_back(tc);
        
        return wc;
    }
    
    // Multiple waiting tasks scenario
    static WorkloadConfig multipleWaiters() {
        WorkloadConfig wc;
        wc.name = "Multiple Waiting Tasks";
        wc.description = "Multiple tasks with different priorities wait for the same resource.";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 50;
        
        ResourceConfig r1;
        r1.id = "R1";
        r1.name = "Shared_Resource";
        wc.resources.push_back(r1);
        
        // Task L (low priority): holds R1
        TaskConfig tl;
        tl.id = "T1";
        tl.name = "Task_Low";
        tl.priority = 1;
        tl.release_time = 0;
        tl.execution_time = 8;
        tl.deadline = 40;
        CriticalSection cs1;
        cs1.resource_id = "R1";
        cs1.duration = 5;
        cs1.offset = 1;
        tl.critical_sections.push_back(cs1);
        wc.tasks.push_back(tl);
        
        // Task M (medium): needs R1
        TaskConfig tm;
        tm.id = "T2";
        tm.name = "Task_Med";
        tm.priority = 5;
        tm.release_time = 2;
        tm.execution_time = 4;
        tm.deadline = 25;
        CriticalSection cs2;
        cs2.resource_id = "R1";
        cs2.duration = 2;
        cs2.offset = 0;
        tm.critical_sections.push_back(cs2);
        wc.tasks.push_back(tm);
        
        // Task H (high): needs R1
        TaskConfig th;
        th.id = "T3";
        th.name = "Task_High";
        th.priority = 10;
        th.release_time = 3;
        th.execution_time = 3;
        th.deadline = 15;
        CriticalSection cs3;
        cs3.resource_id = "R1";
        cs3.duration = 1;
        cs3.offset = 0;
        th.critical_sections.push_back(cs3);
        wc.tasks.push_back(th);
        
        // Task VH (very high): needs R1
        TaskConfig tvh;
        tvh.id = "T4";
        tvh.name = "Task_VHigh";
        tvh.priority = 15;
        tvh.release_time = 4;
        tvh.execution_time = 2;
        tvh.deadline = 12;
        CriticalSection cs4;
        cs4.resource_id = "R1";
        cs4.duration = 1;
        cs4.offset = 0;
        tvh.critical_sections.push_back(cs4);
        wc.tasks.push_back(tvh);
        
        return wc;
    }
    
    // Multiple resources / nested blocking scenario
    static WorkloadConfig nestedBlocking() {
        WorkloadConfig wc;
        wc.name = "Nested Blocking";
        wc.description = "Multiple mutexes with dependencies between tasks, "
                          "testing transitive priority inheritance.";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 60;
        
        ResourceConfig r1, r2;
        r1.id = "R1"; r1.name = "Resource_1";
        r2.id = "R2"; r2.name = "Resource_2";
        wc.resources.push_back(r1);
        wc.resources.push_back(r2);
        
        // Task L holds R1
        TaskConfig tl;
        tl.id = "T1";
        tl.name = "Task_L";
        tl.priority = 1;
        tl.release_time = 0;
        tl.execution_time = 10;
        tl.deadline = 50;
        CriticalSection cs_l;
        cs_l.resource_id = "R1";
        cs_l.duration = 6;
        cs_l.offset = 1;
        tl.critical_sections.push_back(cs_l);
        wc.tasks.push_back(tl);
        
        // Task M holds R2, needs R1 (nested)
        TaskConfig tm;
        tm.id = "T2";
        tm.name = "Task_M";
        tm.priority = 5;
        tm.release_time = 1;
        tm.execution_time = 8;
        tm.deadline = 35;
        CriticalSection cs_m1;
        cs_m1.resource_id = "R2";
        cs_m1.duration = 5;
        cs_m1.offset = 0;
        tm.critical_sections.push_back(cs_m1);
        wc.tasks.push_back(tm);
        
        // Task H needs R2
        TaskConfig th;
        th.id = "T3";
        th.name = "Task_H";
        th.priority = 10;
        th.release_time = 2;
        th.execution_time = 4;
        th.deadline = 20;
        CriticalSection cs_h;
        cs_h.resource_id = "R2";
        cs_h.duration = 2;
        cs_h.offset = 0;
        th.critical_sections.push_back(cs_h);
        wc.tasks.push_back(th);
        
        return wc;
    }
    
    // Deadline miss scenario
    static WorkloadConfig deadlineMiss() {
        WorkloadConfig wc;
        wc.name = "Deadline Miss Scenario";
        wc.description = "A high-priority task has a tight deadline that may be "
                          "missed due to resource blocking.";
        wc.protocol = Protocol::NONE;
        wc.simulation_limit = 40;
        
        ResourceConfig r1;
        r1.id = "R1";
        r1.name = "Critical_Resource";
        wc.resources.push_back(r1);
        
        // Task L: holds resource for a long time
        TaskConfig tl;
        tl.id = "TL";
        tl.name = "Task_L";
        tl.priority = 1;
        tl.release_time = 0;
        tl.execution_time = 12;
        tl.deadline = 30;
        CriticalSection cs_l;
        cs_l.resource_id = "R1";
        cs_l.duration = 8;
        cs_l.offset = 1;
        tl.critical_sections.push_back(cs_l);
        wc.tasks.push_back(tl);
        
        // Task M: CPU-bound interferer
        TaskConfig tm;
        tm.id = "TM";
        tm.name = "Task_M";
        tm.priority = 5;
        tm.release_time = 2;
        tm.execution_time = 6;
        tm.deadline = 25;
        wc.tasks.push_back(tm);
        
        // Task H: tight deadline, needs R1
        TaskConfig th;
        th.id = "TH";
        th.name = "Task_H";
        th.priority = 10;
        th.release_time = 3;
        th.execution_time = 3;
        th.deadline = 8;  // Very tight: must complete by time 11
        CriticalSection cs_h;
        cs_h.resource_id = "R1";
        cs_h.duration = 1;
        cs_h.offset = 0;
        th.critical_sections.push_back(cs_h);
        wc.tasks.push_back(th);
        
        return wc;
    }
};

} // namespace pg

#endif // PRIORITYGUARD_WORKLOAD_GENERATOR_H

