import unittest
import os
import sys

# Add frontend path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "frontend")))
import engine_client

class TestPriorityGuardIntegration(unittest.TestCase):
    
    def test_preset_retrieval(self):
        """Test retrieving built-in scenario presets."""
        presets = ["classic_inversion", "no_inversion", "multiple_waiters", "nested_blocking", "deadline_miss"]
        for p in presets:
            res = engine_client.get_preset_workload(p)
            self.assertEqual(res.get("status"), "success", f"Failed to retrieve preset {p}")
            self.assertIn("workload", res)
            self.assertGreater(len(res["workload"]["tasks"]), 0)

    def test_classic_inversion_simulation(self):
        """Verify baseline produces priority inversion and PIP mitigates it."""
        preset = engine_client.get_preset_workload("classic_inversion")
        workload = preset["workload"]
        
        # Run comparison
        comp = engine_client.compare_protocols(workload)
        self.assertEqual(comp.get("status"), "success")
        
        baseline = comp["baseline"]
        pip = comp["pip"]
        pcp = comp["pcp"]
        
        # 1. Baseline must detect inversion
        self.assertGreater(len(baseline["inversions"]), 0, "Baseline should identify priority inversion")
        self.assertGreater(baseline["inversions"][0]["interference_time"], 0)
        
        # 2. PIP must achieve better or equal response time for high-priority task TH
        th_base = next(tr for tr in baseline["task_results"] if tr["task_id"] == "TH")
        th_pip = next(tr for tr in pip["task_results"] if tr["task_id"] == "TH")
        
        self.assertLess(th_pip["response_time"], th_base["response_time"], "PIP should improve TH response time")
        self.assertGreater(pip["priority_inheritance_events"], 0, "PIP must record inheritance events")

    def test_validation_rejects_malformed_workloads(self):
        """Verify validation correctly catches invalid inputs."""
        bad_workload = {
            "name": "Bad",
            "protocol": "NONE",
            "simulation_limit": 50,
            "resources": [{"id": "R1", "name": "R1"}],
            "tasks": [
                {
                    "id": "T1",
                    "priority": 1,
                    "release_time": 0,
                    "execution_time": 5,
                    "deadline": 10,
                    "critical_sections": [
                        # Unknown resource
                        {"resource_id": "R_NON_EXISTENT", "duration": 2, "offset": 0}
                    ]
                }
            ]
        }
        res = engine_client.validate_workload(bad_workload)
        val = res.get("validation", {})
        self.assertFalse(val.get("valid"))
        self.assertGreater(len(val.get("errors", [])), 0)

    def test_batch_experiments(self):
        """Test reproducible batch experiments using fixed seed."""
        cfg = {
            "num_tasks": 4,
            "num_resources": 2,
            "num_workloads": 5,
            "seed": 12345,
            "resource_sharing_prob": 0.6,
            "deadline_tightness": 2.0,
            "min_execution": 3,
            "max_execution": 8,
            "min_cs_length": 1,
            "max_cs_length": 3,
            "max_release_time": 5,
            "min_priority": 1,
            "max_priority": 10
        }
        res1 = engine_client.run_batch_experiment(cfg)
        res2 = engine_client.run_batch_experiment(cfg)
        
        self.assertEqual(res1.get("status"), "success")
        self.assertEqual(res2.get("status"), "success")
        
        # Results should be deterministic given the same seed
        self.assertEqual(res1["aggregate"]["total_deadline_misses_baseline"], 
                         res2["aggregate"]["total_deadline_misses_baseline"])

if __name__ == "__main__":
    unittest.main()

