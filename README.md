# PriorityGuard: Automated Priority Inversion Analysis & Resource-Sharing Protocol Framework

PriorityGuard is an interactive simulation and analysis framework designed for real-time operating systems education and research. It models preemptive fixed-priority task sets, shared mutex contention, priority inversion episodes, and compares mitigation protocols (**Priority Inheritance Protocol - PIP** and **Priority Ceiling Protocol - PCP**).

> **Important**: This framework is an educational and analytical discrete-event simulator. It simulates task scheduling and resource-sharing behavior according to formal RTOS semantics; it does not alter real operating system kernel threads or manipulate actual Windows scheduler priorities.

---

## Key Features

1. **High-Performance C++ Simulation Engine (C++17)**:
   - Discrete-event single-core preemptive fixed-priority scheduling.
   - Explicit mutex ownership, critical section modeling, and deterministic tie-breaking.
   - Comprehensive event logging with state transitions (`NOT_RELEASED`, `READY`, `RUNNING`, `BLOCKED`, `COMPLETED`).
2. **Automated Priority Inversion Detector**:
   - Trace-based causal analysis distinguishing direct resource blocking from intermediate preemption interference.
   - Reconstructs causal chains: $T_L \text{ holds } R_1 \rightarrow T_H \text{ blocks } \rightarrow T_M \text{ preempts } T_L \rightarrow T_H \text{ delayed}$.
3. **Rigorous Protocol Implementations**:
   - **Baseline**: Standard mutex blocking with no inheritance.
   - **Priority Inheritance Protocol (PIP)**: Dynamic effective priority boosting and transitive inheritance across nested dependencies.
   - **Priority Ceiling Protocol (PCP)**: Static ceiling pre-computation, system ceiling admission checks, and ceiling-induced blocking.
4. **Interactive 8-Page Streamlit Dashboard**:
   - **Page A (Overview)**: Summary metrics, active workload telemetry, and top inversion events.
   - **Page B (Editor)**: Full interactive CRUD task editor with critical section offsets and duration validation.
   - **Page C (Workspace)**: Time-step inspection and chronological event logs.
   - **Page D (Execution Timeline)**: High-resolution Plotly Gantt charts displaying task states, inheritances, and deadline markers.
   - **Page E (Inversion Analysis)**: Detailed diagnosis of each detected episode with duration metrics.
   - **Page F (Protocol Comparison)**: Multi-protocol KPI bar charts and honest evidence-based recommendations.
   - **Page G (Experiment Lab)**: Reproducible synthetic workload generator with fixed random seeds.
   - **Page H (Reports & Exports)**: JSON workload export, full trace export, CSV event logs, and academic Markdown reports.

---

## Directory Structure

```text
PriorityGuard/
├── engine/
│   ├── include/
│   │   ├── types.h              # Data models, enumerations, events
│   │   ├── task.h               # Runtime task state & execution tracking
│   │   ├── mutex_manager.h      # Mutex ownership, queues, PCP ceilings
│   │   ├── scheduler.h          # Preemptive scheduler & simulation loop
│   │   ├── detector.h           # Priority inversion causal detector
│   │   ├── recommender.h        # Rule-based diagnostic recommendations
│   │   ├── workload_generator.h # Synthetic workload generator & presets
│   │   └── json_io.h            # Schema-validated JSON I/O
│   ├── src/
│   │   └── main.cpp             # Engine entry point (CLI/Subprocess bridge)
│   ├── tests/
│   │   └── test_runner.cpp      # C++ Unit tests
│   └── third_party/
│       └── json.hpp             # Modern JSON for C++
├── frontend/
│   ├── app.py                   # Streamlit 8-page interactive UI
│   ├── engine_client.py         # Subprocess JSON bridge to C++ engine
│   └── visualizations.py        # Plotly Gantt & comparison charts
├── tests/
│   └── test_integration.py     # Python end-to-end integration tests
├── docs/                        # Architecture & academic guides
├── CMakeLists.txt               # CMake configuration
├── requirements.txt             # Python dependencies
├── run_priorityguard.bat        # 1-Click Windows launch script
└── README.md
```

---

## Prerequisites & Installation (Windows 10 / 11)

### 1. Prerequisites
- **Python 3.9+** (Tested on Python 3.10, 3.11, 3.14)
- **C++17 Compiler**: MinGW-w64 (`g++`), MSVC, or Clang.
- Windows Command Prompt or PowerShell.

### 2. Quick Launch
Run the automated Windows batch script from the `PriorityGuard/` directory:
```cmd
run_priorityguard.bat
```
This script will:
1. Verify Python and `g++`.
2. Automatically compile the C++ simulation engine if the binary is missing.
3. Verify or install Python packages (`streamlit`, `plotly`, `pandas`).
4. Launch the Streamlit dashboard on `http://localhost:8501`.

### 3. Manual Build & Run
If you prefer building and running manually:

#### Build C++ Engine:
```cmd
g++ -std=c++17 -O2 -o engine\priorityguard_engine.exe engine\src\main.cpp -Iengine\include -Iengine\third_party
```

#### Install Python Dependencies:
```cmd
python -m pip install -r requirements.txt
```

#### Run Unit & Integration Tests:
```cmd
g++ -std=c++17 -O2 -o engine\tests\test_runner.exe engine\tests\test_runner.cpp -Iengine\include -Iengine\third_party
engine\tests\test_runner.exe
python tests\test_integration.py
```

#### Start Frontend:
```cmd
cd frontend
streamlit run app.py
```

---

## Testing & Validation Summary
All test suites pass deterministically:
- **C++ Core Unit Tests (`test_runner.cpp`)**: 4/4 passing (Classic inversion, No-inversion baseline, Priority Ceiling Protocol, and Nested transitive inheritance).
- **Python Integration Tests (`test_integration.py`)**: 4/4 passing (Preset loading, simulation comparison, invalid input rejection, and reproducible random seed generation).

