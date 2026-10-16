/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * recommender.h - Rule-based diagnosis and recommendation engine
 *
 * Interprets simulation results to suggest improvements.
 * Recommendations are based on measurable criteria from actual simulation data,
 * not on heuristics or ML models.
 */

#ifndef PRIORITYGUARD_RECOMMENDER_H
#define PRIORITYGUARD_RECOMMENDER_H

#include "types.h"
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

namespace pg {

class Recommender {
public:
    // Generate recommendations based on simulation results
    std::vector<Recommendation> analyze(const SimulationResults& baseline,
                                         const SimulationResults& pip_results,
                                         const SimulationResults& pcp_results) {
        std::vector<Recommendation> recs;
        
        // Rule 1: Check if priority inversions exist in baseline
        if (!baseline.inversions.empty()) {
            // Find total inversion time
            int total_inv_time = 0;
            for (const auto& inv : baseline.inversions) {
                total_inv_time += inv.interference_time;
            }
            
            if (total_inv_time > 0) {
                Recommendation r;
                r.problem = "Priority inversion detected in baseline scheduling";
                
                std::stringstream ss;
                ss << baseline.inversions.size() << " inversion scenario(s) detected. "
                   << "Total interference delay: " << total_inv_time << " time units. ";
                for (const auto& inv : baseline.inversions) {
                    ss << "Task " << inv.high_task_id << " blocked by " 
                       << inv.low_task_id << " on " << inv.resource_id 
                       << " with " << inv.interference_time << " units interference. ";
                }
                r.evidence = ss.str();
                
                // Compare PIP improvement
                if (pip_results.total_priority_inversion_time < total_inv_time) {
                    r.suggestion = "Enable Priority Inheritance Protocol (PIP)";
                    
                    std::stringstream ben;
                    ben << "PIP reduced blocking: avg response time "
                        << baseline.avg_response_time << " -> " 
                        << pip_results.avg_response_time << ". "
                        << "Deadline misses: " << baseline.deadline_misses 
                        << " -> " << pip_results.deadline_misses << ".";
                    r.benefit = ben.str();
                } else {
                    r.suggestion = "Consider PIP or PCP for this workload";
                    r.benefit = "May reduce blocking delays for high-priority tasks";
                }
                
                r.tradeoff = "PIP introduces runtime overhead for priority tracking "
                             "and may cause additional context switches";
                recs.push_back(r);
            }
        }
        
        // Rule 2: Compare PIP vs PCP
        if (pip_results.total_tasks > 0 && pcp_results.total_tasks > 0) {
            Recommendation r;
            r.problem = "Protocol comparison for resource contention";
            
            std::stringstream ev;
            ev << "PIP: avg_response=" << pip_results.avg_response_time
               << " max_block=" << pip_results.max_blocking_duration
               << " deadline_misses=" << pip_results.deadline_misses
               << " | PCP: avg_response=" << pcp_results.avg_response_time
               << " max_block=" << pcp_results.max_blocking_duration
               << " deadline_misses=" << pcp_results.deadline_misses;
            r.evidence = ev.str();
            
            if (pcp_results.avg_response_time < pip_results.avg_response_time &&
                pcp_results.deadline_misses <= pip_results.deadline_misses) {
                r.suggestion = "PCP performs better than PIP for this workload";
                r.benefit = "Lower average response time and fewer/equal deadline misses";
                r.tradeoff = "PCP requires knowing all resource ceilings at design time "
                             "and may cause unnecessary ceiling-induced blocking";
            } else if (pip_results.avg_response_time < pcp_results.avg_response_time) {
                r.suggestion = "PIP performs better than PCP for this workload";
                r.benefit = "Lower average response time";
                r.tradeoff = "PIP may allow transitive blocking chains, "
                             "potentially increasing worst-case latency";
            } else {
                r.suggestion = "PIP and PCP show similar performance for this workload";
                r.benefit = "Either protocol adequately handles resource contention";
                r.tradeoff = "Choose based on implementation complexity and design constraints";
            }
            recs.push_back(r);
        }
        
        // Rule 3: Check for high blocking times
        if (baseline.max_blocking_duration > 0) {
            // Find tasks with high blocking
            for (const auto& tr : baseline.task_results) {
                if (tr.blocked_time > 0) {
                    int exec_ratio = 0;
                    // Check if blocking is a significant portion of response time
                    if (tr.response_time > 0) {
                        exec_ratio = (tr.blocked_time * 100) / tr.response_time;
                    }
                    
                    if (exec_ratio > 30) { // Blocking > 30% of response time
                        Recommendation r;
                        r.problem = "Task " + tr.task_id + " spends " + 
                                   std::to_string(exec_ratio) + 
                                   "% of response time blocked";
                        r.evidence = "Blocked time: " + std::to_string(tr.blocked_time) +
                                    ", Response time: " + std::to_string(tr.response_time) +
                                    ", Max blocking duration: " + 
                                    std::to_string(tr.max_blocking_duration);
                        r.suggestion = "Reduce critical section lengths for resources "
                                      "used by this task's blocking dependencies, "
                                      "or consider restructuring resource sharing";
                        r.benefit = "Reduced blocking time leads to lower response times "
                                   "and fewer potential deadline misses";
                        r.tradeoff = "Shorter critical sections may require more complex "
                                    "application logic or additional synchronization";
                        recs.push_back(r);
                    }
                }
            }
        }
        
        // Rule 4: Check for deadline misses
        if (baseline.deadline_misses > 0) {
            bool pip_helps = (pip_results.deadline_misses < baseline.deadline_misses);
            bool pcp_helps = (pcp_results.deadline_misses < baseline.deadline_misses);
            
            if (!pip_helps && !pcp_helps) {
                Recommendation r;
                r.problem = std::to_string(baseline.deadline_misses) + 
                           " deadline miss(es) not resolved by protocol changes";
                r.evidence = "Baseline: " + std::to_string(baseline.deadline_misses) +
                            " misses, PIP: " + std::to_string(pip_results.deadline_misses) +
                            " misses, PCP: " + std::to_string(pcp_results.deadline_misses) +
                            " misses";
                r.suggestion = "Review deadline and execution-time assumptions. "
                              "The workload may be inherently infeasible with "
                              "current task parameters";
                r.benefit = "Identifying fundamental feasibility issues avoids "
                           "wasting effort on protocol tuning";
                r.tradeoff = "May require redesigning the task set, relaxing "
                            "deadlines, or reducing execution demands";
                recs.push_back(r);
            }
        }
        
        // Rule 5: No inversion detected
        if (baseline.inversions.empty() && baseline.total_blocked_time == 0) {
            Recommendation r;
            r.problem = "No priority inversion or resource blocking detected";
            r.evidence = "No tasks were blocked on shared resources during simulation";
            r.suggestion = "The current workload does not demonstrate priority inversion. "
                          "No protocol changes are needed for this configuration";
            r.benefit = "Confirms workload operates correctly under baseline scheduling";
            r.tradeoff = "Different workloads or timing conditions may still exhibit "
                        "priority inversion";
            recs.push_back(r);
        }
        
        return recs;
    }
};

} // namespace pg

#endif // PRIORITYGUARD_RECOMMENDER_H

