/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * json_io.h - JSON serialization/deserialization for engine I/O
 *
 * Handles conversion between C++ types and JSON using nlohmann/json.
 * Defines the JSON schema for communication between engine and frontend.
 */

#ifndef PRIORITYGUARD_JSON_IO_H
#define PRIORITYGUARD_JSON_IO_H

#include "types.h"
#include "../third_party/json.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace pg {

// ============================================================================
// JSON -> C++ (Deserialization / Input Parsing)
// ============================================================================

inline CriticalSection parseCriticalSection(const json& j) {
    CriticalSection cs;
    cs.resource_id = j.at("resource_id").get<std::string>();
    cs.duration = j.at("duration").get<int>();
    cs.offset = j.at("offset").get<int>();
    return cs;
}

inline TaskConfig parseTaskConfig(const json& j) {
    TaskConfig tc;
    tc.id = j.at("id").get<std::string>();
    tc.name = j.value("name", tc.id);
    tc.priority = j.at("priority").get<int>();
    tc.release_time = j.value("release_time", 0);
    tc.execution_time = j.at("execution_time").get<int>();
    tc.deadline = j.at("deadline").get<int>();
    
    if (j.count("critical_sections")) {
        for (const auto& csj : j.at("critical_sections")) {
            tc.critical_sections.push_back(parseCriticalSection(csj));
        }
    }
    
    return tc;
}

inline ResourceConfig parseResourceConfig(const json& j) {
    ResourceConfig rc;
    rc.id = j.at("id").get<std::string>();
    rc.name = j.value("name", rc.id);
    return rc;
}

inline WorkloadConfig parseWorkloadConfig(const json& j) {
    WorkloadConfig wc;
    wc.name = j.value("name", "Unnamed");
    wc.description = j.value("description", "");
    wc.protocol = protocolFromString(j.value("protocol", "NONE"));
    wc.simulation_limit = j.value("simulation_limit", 500);
    
    if (j.count("tasks")) {
        for (const auto& tj : j.at("tasks")) {
            wc.tasks.push_back(parseTaskConfig(tj));
        }
    }
    
    if (j.count("resources")) {
        for (const auto& rj : j.at("resources")) {
            wc.resources.push_back(parseResourceConfig(rj));
        }
    }
    
    return wc;
}

inline GeneratorConfig parseGeneratorConfig(const json& j) {
    GeneratorConfig gc;
    gc.num_tasks = j.value("num_tasks", 5);
    gc.num_resources = j.value("num_resources", 2);
    gc.min_priority = j.value("min_priority", 1);
    gc.max_priority = j.value("max_priority", 10);
    gc.min_execution = j.value("min_execution", 3);
    gc.max_execution = j.value("max_execution", 15);
    gc.min_cs_length = j.value("min_cs_length", 1);
    gc.max_cs_length = j.value("max_cs_length", 5);
    gc.resource_sharing_prob = j.value("resource_sharing_prob", 0.5);
    gc.max_release_time = j.value("max_release_time", 10);
    gc.deadline_tightness = j.value("deadline_tightness", 2.0);
    gc.num_workloads = j.value("num_workloads", 10);
    gc.seed = j.value("seed", 42u);
    return gc;
}

// ============================================================================
// C++ -> JSON (Serialization / Output)
// ============================================================================

inline json criticalSectionToJson(const CriticalSection& cs) {
    json j;
    j["resource_id"] = cs.resource_id;
    j["duration"] = cs.duration;
    j["offset"] = cs.offset;
    return j;
}

inline json taskConfigToJson(const TaskConfig& tc) {
    json j;
    j["id"] = tc.id;
    j["name"] = tc.name;
    j["priority"] = tc.priority;
    j["release_time"] = tc.release_time;
    j["execution_time"] = tc.execution_time;
    j["deadline"] = tc.deadline;
    j["critical_sections"] = json::array();
    for (const auto& cs : tc.critical_sections) {
        j["critical_sections"].push_back(criticalSectionToJson(cs));
    }
    return j;
}

inline json resourceConfigToJson(const ResourceConfig& rc) {
    json j;
    j["id"] = rc.id;
    j["name"] = rc.name;
    return j;
}

inline json workloadConfigToJson(const WorkloadConfig& wc) {
    json j;
    j["name"] = wc.name;
    j["description"] = wc.description;
    j["protocol"] = protocolToString(wc.protocol);
    j["simulation_limit"] = wc.simulation_limit;
    j["tasks"] = json::array();
    for (const auto& tc : wc.tasks) {
        j["tasks"].push_back(taskConfigToJson(tc));
    }
    j["resources"] = json::array();
    for (const auto& rc : wc.resources) {
        j["resources"].push_back(resourceConfigToJson(rc));
    }
    return j;
}

inline json simEventToJson(const SimEvent& e) {
    json j;
    j["time"] = e.time;
    j["type"] = eventTypeToString(e.type);
    j["task_id"] = e.task_id;
    j["resource_id"] = e.resource_id;
    j["details"] = e.details;
    j["base_priority"] = e.base_priority;
    j["effective_priority"] = e.effective_priority;
    return j;
}

inline json taskResultToJson(const TaskResult& tr) {
    json j;
    j["task_id"] = tr.task_id;
    j["task_name"] = tr.task_name;
    j["base_priority"] = tr.base_priority;
    j["release_time"] = tr.release_time;
    j["deadline"] = tr.deadline;
    j["start_time"] = tr.start_time;
    j["completion_time"] = tr.completion_time;
    j["response_time"] = tr.response_time;
    j["waiting_time"] = tr.waiting_time;
    j["blocked_time"] = tr.blocked_time;
    j["deadline_missed"] = tr.deadline_missed;
    j["max_blocking_duration"] = tr.max_blocking_duration;
    j["priority_inversions"] = tr.priority_inversions;
    return j;
}

inline json inversionRecordToJson(const InversionRecord& inv) {
    json j;
    j["start_time"] = inv.start_time;
    j["end_time"] = inv.end_time;
    j["direct_blocking_time"] = inv.direct_blocking_time;
    j["interference_time"] = inv.interference_time;
    j["high_task_id"] = inv.high_task_id;
    j["low_task_id"] = inv.low_task_id;
    j["resource_id"] = inv.resource_id;
    j["interfering_tasks"] = inv.interfering_tasks;
    j["caused_deadline_miss"] = inv.caused_deadline_miss;
    j["causal_chain"] = inv.causal_chain;
    return j;
}

inline json recommendationToJson(const Recommendation& r) {
    json j;
    j["problem"] = r.problem;
    j["evidence"] = r.evidence;
    j["suggestion"] = r.suggestion;
    j["benefit"] = r.benefit;
    j["tradeoff"] = r.tradeoff;
    return j;
}

inline json simulationResultsToJson(const SimulationResults& sr) {
    json j;
    j["protocol"] = protocolToString(sr.protocol);
    j["total_time"] = sr.total_time;
    j["context_switches"] = sr.context_switches;
    j["priority_inheritance_events"] = sr.priority_inheritance_events;
    j["protocol_blocking_events"] = sr.protocol_blocking_events;
    j["cpu_utilization"] = sr.cpu_utilization;
    j["avg_response_time"] = sr.avg_response_time;
    j["worst_response_time"] = sr.worst_response_time;
    j["avg_waiting_time"] = sr.avg_waiting_time;
    j["total_blocked_time"] = sr.total_blocked_time;
    j["max_blocking_duration"] = sr.max_blocking_duration;
    j["total_priority_inversion_time"] = sr.total_priority_inversion_time;
    j["deadline_misses"] = sr.deadline_misses;
    j["completed_tasks"] = sr.completed_tasks;
    j["total_tasks"] = sr.total_tasks;
    
    j["task_results"] = json::array();
    for (const auto& tr : sr.task_results) {
        j["task_results"].push_back(taskResultToJson(tr));
    }
    
    j["inversions"] = json::array();
    for (const auto& inv : sr.inversions) {
        j["inversions"].push_back(inversionRecordToJson(inv));
    }
    
    j["events"] = json::array();
    for (const auto& e : sr.events) {
        j["events"].push_back(simEventToJson(e));
    }
    
    return j;
}

// ============================================================================
// Input Validation
// ============================================================================

struct ValidationResult {
    bool valid;
    std::vector<std::string> errors;
};

inline ValidationResult validateWorkload(const WorkloadConfig& wc) {
    ValidationResult vr;
    vr.valid = true;
    
    if (wc.tasks.empty()) {
        vr.errors.push_back("Workload must have at least one task");
        vr.valid = false;
    }
    
    // Build set of valid resource IDs and check for duplicates
    std::vector<std::string> valid_resources;
    for (const auto& r : wc.resources) {
        if (r.id.empty()) {
            vr.errors.push_back("Resource ID cannot be empty");
            vr.valid = false;
        }
        for (const auto& existing : valid_resources) {
            if (existing == r.id) {
                vr.errors.push_back("Duplicate resource ID: " + r.id);
                vr.valid = false;
            }
        }
        valid_resources.push_back(r.id);
    }
    
    // Check for duplicate task IDs
    std::vector<std::string> task_ids;
    for (const auto& t : wc.tasks) {
        for (const auto& existing : task_ids) {
            if (existing == t.id) {
                vr.errors.push_back("Duplicate task ID: " + t.id);
                vr.valid = false;
            }
        }
        task_ids.push_back(t.id);
    }
    
    // Validate each task
    for (const auto& t : wc.tasks) {
        if (t.id.empty()) {
            vr.errors.push_back("Task ID cannot be empty");
            vr.valid = false;
        }
        if (t.execution_time <= 0) {
            vr.errors.push_back("Task " + t.id + ": execution_time must be positive");
            vr.valid = false;
        }
        if (t.deadline <= 0) {
            vr.errors.push_back("Task " + t.id + ": deadline must be positive");
            vr.valid = false;
        }
        if (t.release_time < 0) {
            vr.errors.push_back("Task " + t.id + ": release_time cannot be negative");
            vr.valid = false;
        }
        if (t.priority <= 0) {
            vr.errors.push_back("Task " + t.id + ": priority must be positive");
            vr.valid = false;
        }
        
        // Validate critical sections
        for (size_t i = 0; i < t.critical_sections.size(); i++) {
            const auto& cs = t.critical_sections[i];
            
            // Check resource reference
            bool found = false;
            for (const auto& rid : valid_resources) {
                if (rid == cs.resource_id) { found = true; break; }
            }
            if (!found) {
                vr.errors.push_back("Task " + t.id + ": critical section references "
                                   "unknown resource '" + cs.resource_id + "'");
                vr.valid = false;
            }
            
            if (cs.duration <= 0) {
                vr.errors.push_back("Task " + t.id + ": critical section duration "
                                   "must be positive");
                vr.valid = false;
            }
            if (cs.offset < 0) {
                vr.errors.push_back("Task " + t.id + ": critical section offset "
                                   "cannot be negative");
                vr.valid = false;
            }
            if (cs.offset + cs.duration > t.execution_time) {
                vr.errors.push_back("Task " + t.id + ": critical section exceeds "
                                   "total execution time (offset=" + std::to_string(cs.offset) + 
                                   " + duration=" + std::to_string(cs.duration) + 
                                   " > execution=" + std::to_string(t.execution_time) + ")");
                vr.valid = false;
            }

            // Check for overlapping critical sections on the same task
            for (size_t j = i + 1; j < t.critical_sections.size(); j++) {
                const auto& other = t.critical_sections[j];
                int s1 = cs.offset, e1 = cs.offset + cs.duration;
                int s2 = other.offset, e2 = other.offset + other.duration;
                if (std::max(s1, s2) < std::min(e1, e2)) {
                    vr.errors.push_back("Task " + t.id + ": critical sections overlap in execution (" +
                                       cs.resource_id + " [" + std::to_string(s1) + ".." + std::to_string(e1) + 
                                       "] and " + other.resource_id + " [" + std::to_string(s2) + ".." + 
                                       std::to_string(e2) + "])");
                    vr.valid = false;
                }
            }
        }
    }
    
    if (wc.simulation_limit <= 0) {
        vr.errors.push_back("simulation_limit must be positive");
        vr.valid = false;
    }
    
    return vr;
}

inline json validationResultToJson(const ValidationResult& vr) {
    json j;
    j["valid"] = vr.valid;
    j["errors"] = vr.errors;
    return j;
}

// ============================================================================
// File I/O Helpers
// ============================================================================

inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot write to file: " + path);
    }
    f << content;
}

} // namespace pg

#endif // PRIORITYGUARD_JSON_IO_H

