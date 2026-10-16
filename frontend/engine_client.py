import json
import subprocess
import os
from typing import Dict, Any, Optional

ENGINE_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "engine", "priorityguard_engine.exe")
)

def run_engine_command(command_payload: Dict[str, Any]) -> Dict[str, Any]:
    """Execute a command against the C++ PriorityGuard simulation engine."""
    if not os.path.exists(ENGINE_PATH):
        return {
            "status": "error",
            "message": f"Engine binary not found at {ENGINE_PATH}. Please compile first."
        }
    
    input_json = json.dumps(command_payload)
    try:
        process = subprocess.Popen(
            [ENGINE_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding='utf-8'
        )
        stdout, stderr = process.communicate(input=input_json, timeout=30)
        
        if process.returncode != 0 and not stdout:
            return {
                "status": "error",
                "message": f"Engine exited with code {process.returncode}: {stderr}"
            }
        
        return json.loads(stdout)
    except subprocess.TimeoutExpired:
        process.kill()
        return {"status": "error", "message": "Simulation engine timed out after 30 seconds."}
    except Exception as e:
        return {"status": "error", "message": f"Failed to execute engine: {str(e)}"}

def get_preset_workload(preset_name: str) -> Dict[str, Any]:
    """Fetch one of the built-in scenario workloads."""
    return run_engine_command({
        "command": "preset",
        "preset": preset_name
    })

def simulate_workload(workload: Dict[str, Any]) -> Dict[str, Any]:
    """Run a single simulation."""
    return run_engine_command({
        "command": "simulate",
        "workload": workload
    })

def compare_protocols(workload: Dict[str, Any]) -> Dict[str, Any]:
    """Run the comparison across baseline, PIP, and PCP."""
    return run_engine_command({
        "command": "compare",
        "workload": workload
    })

def validate_workload(workload: Dict[str, Any]) -> Dict[str, Any]:
    """Validate a workload configuration."""
    return run_engine_command({
        "command": "validate",
        "workload": workload
    })

def generate_workloads(generator_config: Dict[str, Any]) -> Dict[str, Any]:
    """Generate random synthetic workloads."""
    return run_engine_command({
        "command": "generate",
        "config": generator_config
    })

def run_batch_experiment(generator_config: Dict[str, Any]) -> Dict[str, Any]:
    """Run batch experiment across generated workloads."""
    return run_engine_command({
        "command": "batch",
        "config": generator_config
    })

