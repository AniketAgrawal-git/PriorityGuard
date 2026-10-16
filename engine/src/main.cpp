/*
 * PriorityGuard - Priority Inversion Analysis Framework
 * main.cpp - Engine entry point
 *
 * The engine reads a JSON command from stdin (or file), processes it,
 * and writes JSON results to stdout. This is the interface between the
 * C++ simulation engine and the Python/Streamlit frontend.
 *
 * Supported commands:
 *   simulate     - Run simulation with a workload config
 *   compare      - Run same workload under NONE, PIP, PCP
 *   validate     - Validate a workload configuration
 *   generate     - Generate random workloads
 *   preset       - Get a predefined scenario
 *   batch        - Run batch experiment
 *
 * Usage:
 *   echo '{"command":"simulate","workload":{...}}' | priorityguard_engine.exe
 *   priorityguard_engine.exe input.json
 */

#include "../include/types.h"
#include "../include/task.h"
#include "../include/mutex_manager.h"
#include "../include/scheduler.h"
#include "../include/detector.h"
#include "../include/recommender.h"
#include "../include/workload_generator.h"
#include "../include/json_io.h"
#include "../third_party/json.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace pg;

// Run a single simulation and detect inversions
SimulationResults runSimulation(const WorkloadConfig& wc) {
    Scheduler scheduler;
    scheduler.init(wc);
    SimulationResults results = scheduler.run();
    
    // Detect priority inversions
    InversionDetector detector;
    results.inversions = detector.detect(results.events, results.task_results);
    
    // Calculate total inversion time
    int total_inv_time = 0;
    for (const auto& inv : results.inversions) {
        total_inv_time += (inv.end_time - inv.start_time);
    }
    results.total_priority_inversion_time = total_inv_time;
    
    // Update per-task inversion counts
    for (auto& tr : results.task_results) {
        for (const auto& inv : results.inversions) {
            if (inv.high_task_id == tr.task_id) {
                tr.priority_inversions++;
            }
        }
    }
    
    return results;
}

// Handle 'simulate' command
json handleSimulate(const json& input) {
    try {
        WorkloadConfig wc = parseWorkloadConfig(input.at("workload"));
        
        // Validate
        ValidationResult vr = validateWorkload(wc);
        if (!vr.valid) {
            json response;
            response["status"] = "error";
            response["validation"] = validationResultToJson(vr);
            return response;
        }
        
        SimulationResults results = runSimulation(wc);
        
        json response;
        response["status"] = "success";
        response["results"] = simulationResultsToJson(results);
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Simulation error: ") + e.what();
        return response;
    }
}

// Handle 'compare' command - run same workload under all protocols
json handleCompare(const json& input) {
    try {
        WorkloadConfig wc = parseWorkloadConfig(input.at("workload"));
        
        ValidationResult vr = validateWorkload(wc);
        if (!vr.valid) {
            json response;
            response["status"] = "error";
            response["validation"] = validationResultToJson(vr);
            return response;
        }
        
        // Run under each protocol
        json response;
        response["status"] = "success";
        
        // Baseline (NONE)
        wc.protocol = Protocol::NONE;
        SimulationResults baseline = runSimulation(wc);
        response["baseline"] = simulationResultsToJson(baseline);
        
        // PIP
        wc.protocol = Protocol::PIP;
        SimulationResults pip = runSimulation(wc);
        response["pip"] = simulationResultsToJson(pip);
        
        // PCP
        wc.protocol = Protocol::PCP;
        SimulationResults pcp = runSimulation(wc);
        response["pcp"] = simulationResultsToJson(pcp);
        
        // Generate recommendations
        Recommender recommender;
        std::vector<Recommendation> recs = recommender.analyze(baseline, pip, pcp);
        response["recommendations"] = json::array();
        for (const auto& r : recs) {
            response["recommendations"].push_back(recommendationToJson(r));
        }
        
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Compare error: ") + e.what();
        return response;
    }
}

// Handle 'validate' command
json handleValidate(const json& input) {
    try {
        WorkloadConfig wc = parseWorkloadConfig(input.at("workload"));
        ValidationResult vr = validateWorkload(wc);
        
        json response;
        response["status"] = "success";
        response["validation"] = validationResultToJson(vr);
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Validation error: ") + e.what();
        return response;
    }
}

// Handle 'generate' command
json handleGenerate(const json& input) {
    try {
        GeneratorConfig gc;
        if (input.count("config")) {
            gc = parseGeneratorConfig(input.at("config"));
        }
        
        WorkloadGenerator gen;
        std::vector<WorkloadConfig> workloads = gen.generate(gc);
        
        json response;
        response["status"] = "success";
        response["workloads"] = json::array();
        for (const auto& wc : workloads) {
            response["workloads"].push_back(workloadConfigToJson(wc));
        }
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Generate error: ") + e.what();
        return response;
    }
}

// Handle 'preset' command
json handlePreset(const json& input) {
    try {
        std::string preset_name = input.value("preset", "classic_inversion");
        WorkloadConfig wc;
        
        if (preset_name == "classic_inversion") {
            wc = WorkloadGenerator::classicInversion();
        } else if (preset_name == "no_inversion") {
            wc = WorkloadGenerator::noInversion();
        } else if (preset_name == "multiple_waiters") {
            wc = WorkloadGenerator::multipleWaiters();
        } else if (preset_name == "nested_blocking") {
            wc = WorkloadGenerator::nestedBlocking();
        } else if (preset_name == "deadline_miss") {
            wc = WorkloadGenerator::deadlineMiss();
        } else {
            json response;
            response["status"] = "error";
            response["message"] = "Unknown preset: " + preset_name +
                ". Available: classic_inversion, no_inversion, multiple_waiters, "
                "nested_blocking, deadline_miss";
            return response;
        }
        
        json response;
        response["status"] = "success";
        response["workload"] = workloadConfigToJson(wc);
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Preset error: ") + e.what();
        return response;
    }
}

// Handle 'batch' command - generate and run multiple workloads
json handleBatch(const json& input) {
    try {
        GeneratorConfig gc;
        if (input.count("config")) {
            gc = parseGeneratorConfig(input.at("config"));
        }
        
        WorkloadGenerator gen;
        std::vector<WorkloadConfig> workloads = gen.generate(gc);
        
        json response;
        response["status"] = "success";
        response["num_workloads"] = (int)workloads.size();
        response["seed"] = gc.seed;
        
        // Aggregate statistics
        int total_inversions_baseline = 0;
        int total_inversions_pip = 0;
        int total_inversions_pcp = 0;
        int total_deadline_misses_baseline = 0;
        int total_deadline_misses_pip = 0;
        int total_deadline_misses_pcp = 0;
        double total_resp_baseline = 0, total_resp_pip = 0, total_resp_pcp = 0;
        int total_block_baseline = 0, total_block_pip = 0, total_block_pcp = 0;
        int max_block_baseline = 0, max_block_pip = 0, max_block_pcp = 0;
        int workloads_with_inversion = 0;
        
        json workload_results = json::array();
        
        for (size_t i = 0; i < workloads.size(); i++) {
            auto& wc = workloads[i];
            
            // Validate
            ValidationResult vr = validateWorkload(wc);
            if (!vr.valid) continue;
            
            // Run under all protocols
            wc.protocol = Protocol::NONE;
            SimulationResults baseline = runSimulation(wc);
            
            wc.protocol = Protocol::PIP;
            SimulationResults pip = runSimulation(wc);
            
            wc.protocol = Protocol::PCP;
            SimulationResults pcp = runSimulation(wc);
            
            // Accumulate
            total_inversions_baseline += (int)baseline.inversions.size();
            total_inversions_pip += (int)pip.inversions.size();
            total_inversions_pcp += (int)pcp.inversions.size();
            total_deadline_misses_baseline += baseline.deadline_misses;
            total_deadline_misses_pip += pip.deadline_misses;
            total_deadline_misses_pcp += pcp.deadline_misses;
            total_resp_baseline += baseline.avg_response_time;
            total_resp_pip += pip.avg_response_time;
            total_resp_pcp += pcp.avg_response_time;
            total_block_baseline += baseline.total_blocked_time;
            total_block_pip += pip.total_blocked_time;
            total_block_pcp += pcp.total_blocked_time;
            if (baseline.max_blocking_duration > max_block_baseline)
                max_block_baseline = baseline.max_blocking_duration;
            if (pip.max_blocking_duration > max_block_pip)
                max_block_pip = pip.max_blocking_duration;
            if (pcp.max_blocking_duration > max_block_pcp)
                max_block_pcp = pcp.max_blocking_duration;
            
            if (!baseline.inversions.empty()) workloads_with_inversion++;
            
            // Per-workload summary
            json wr;
            wr["workload_index"] = (int)i;
            wr["workload_name"] = wc.name;
            wr["baseline_inversions"] = (int)baseline.inversions.size();
            wr["pip_inversions"] = (int)pip.inversions.size();
            wr["pcp_inversions"] = (int)pcp.inversions.size();
            wr["baseline_deadline_misses"] = baseline.deadline_misses;
            wr["pip_deadline_misses"] = pip.deadline_misses;
            wr["pcp_deadline_misses"] = pcp.deadline_misses;
            wr["baseline_avg_response"] = baseline.avg_response_time;
            wr["pip_avg_response"] = pip.avg_response_time;
            wr["pcp_avg_response"] = pcp.avg_response_time;
            wr["baseline_max_block"] = baseline.max_blocking_duration;
            wr["pip_max_block"] = pip.max_blocking_duration;
            wr["pcp_max_block"] = pcp.max_blocking_duration;
            workload_results.push_back(wr);
        }
        
        int n = (int)workloads.size();
        
        // Aggregate statistics
        json agg;
        agg["workloads_with_inversion"] = workloads_with_inversion;
        agg["inversion_percentage"] = n > 0 ? 
            (workloads_with_inversion * 100.0 / n) : 0.0;
        agg["total_inversions_baseline"] = total_inversions_baseline;
        agg["total_inversions_pip"] = total_inversions_pip;
        agg["total_inversions_pcp"] = total_inversions_pcp;
        agg["total_deadline_misses_baseline"] = total_deadline_misses_baseline;
        agg["total_deadline_misses_pip"] = total_deadline_misses_pip;
        agg["total_deadline_misses_pcp"] = total_deadline_misses_pcp;
        agg["avg_response_baseline"] = n > 0 ? total_resp_baseline / n : 0.0;
        agg["avg_response_pip"] = n > 0 ? total_resp_pip / n : 0.0;
        agg["avg_response_pcp"] = n > 0 ? total_resp_pcp / n : 0.0;
        agg["total_blocked_baseline"] = total_block_baseline;
        agg["total_blocked_pip"] = total_block_pip;
        agg["total_blocked_pcp"] = total_block_pcp;
        agg["max_blocking_baseline"] = max_block_baseline;
        agg["max_blocking_pip"] = max_block_pip;
        agg["max_blocking_pcp"] = max_block_pcp;
        
        response["aggregate"] = agg;
        response["workload_results"] = workload_results;
        
        return response;
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["message"] = std::string("Batch error: ") + e.what();
        return response;
    }
}

int main(int argc, char* argv[]) {
    try {
        std::string input_str;
        
        if (argc > 1) {
            // Read from file
            input_str = readFile(argv[1]);
        } else {
            // Read from stdin
            std::stringstream ss;
            ss << std::cin.rdbuf();
            input_str = ss.str();
        }
        
        if (input_str.empty()) {
            json err;
            err["status"] = "error";
            err["message"] = "No input provided. Send JSON to stdin or specify a file.";
            std::cout << err.dump(2) << std::endl;
            return 1;
        }
        
        json input = json::parse(input_str);
        std::string command = input.value("command", "");
        
        json response;
        
        if (command == "simulate") {
            response = handleSimulate(input);
        } else if (command == "compare") {
            response = handleCompare(input);
        } else if (command == "validate") {
            response = handleValidate(input);
        } else if (command == "generate") {
            response = handleGenerate(input);
        } else if (command == "preset") {
            response = handlePreset(input);
        } else if (command == "batch") {
            response = handleBatch(input);
        } else {
            response["status"] = "error";
            response["message"] = "Unknown command: '" + command + 
                "'. Available: simulate, compare, validate, generate, preset, batch";
        }
        
        std::cout << response.dump(2) << std::endl;
        return 0;
        
    } catch (const json::exception& e) {
        json err;
        err["status"] = "error";
        err["message"] = std::string("JSON parsing error: ") + e.what();
        std::cout << err.dump(2) << std::endl;
        return 1;
    } catch (const std::exception& e) {
        json err;
        err["status"] = "error";
        err["message"] = std::string("Engine error: ") + e.what();
        std::cout << err.dump(2) << std::endl;
        return 1;
    }
}

