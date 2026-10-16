import streamlit as st
import json
import pandas as pd
import os
import sys

# Ensure frontend directory is in path
sys.path.append(os.path.dirname(__file__))

import engine_client
import visualizations

# Set Page Config
st.set_page_config(
    page_title="PriorityGuard | Priority Inversion Analysis Framework",
    page_icon="🛡️",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom Dark Theme Styling
st.markdown("""
<style>
    .main {
        background-color: #0E1117;
        color: #FAFAFA;
    }
    .stMetric {
        background-color: #1E222A;
        padding: 15px;
        border-radius: 8px;
        border: 1px solid #30363D;
    }
    .metric-card {
        background-color: #1E222A;
        border: 1px solid #30363D;
        border-radius: 8px;
        padding: 16px;
        margin-bottom: 12px;
    }
    .causal-box {
        background-color: #161B22;
        border-left: 4px solid #F85149;
        padding: 14px;
        border-radius: 4px;
        font-family: monospace;
        margin: 10px 0px;
    }
    .rec-box {
        background-color: #161B22;
        border-left: 4px solid #58A6FF;
        padding: 14px;
        border-radius: 4px;
        margin: 10px 0px;
    }
</style>
""", unsafe_allow_html=True)

# Session State Initialization
if "current_workload" not in st.session_state:
    preset_res = engine_client.get_preset_workload("classic_inversion")
    if preset_res.get("status") == "success":
        st.session_state.current_workload = preset_res["workload"]
    else:
        st.session_state.current_workload = {
            "name": "Default Workload",
            "description": "Default initial workload",
            "protocol": "NONE",
            "simulation_limit": 50,
            "tasks": [],
            "resources": []
        }

if "last_simulation_results" not in st.session_state:
    st.session_state.last_simulation_results = None

if "last_comparison_results" not in st.session_state:
    st.session_state.last_comparison_results = None

# Sidebar Navigation
st.sidebar.title("🛡️ PriorityGuard")
st.sidebar.caption("Automated Priority Inversion Analysis & Protocol Evaluation Framework")

pages = [
    "Page A: Overview Dashboard",
    "Page B: Task & Resource Editor",
    "Page C: Simulation Workspace",
    "Page D: Execution Timeline",
    "Page E: Inversion Analysis",
    "Page F: Protocol Comparison",
    "Page G: Experiment Lab",
    "Page H: Reports & Exports"
]

selected_page = st.sidebar.radio("Navigation", pages)

st.sidebar.markdown("---")
st.sidebar.subheader("Presets & Quick Load")
preset_options = {
    "Classic Priority Inversion": "classic_inversion",
    "No Inversion": "no_inversion",
    "Multiple Waiting Tasks": "multiple_waiters",
    "Nested Blocking": "nested_blocking",
    "Deadline Miss Scenario": "deadline_miss"
}
selected_preset = st.sidebar.selectbox("Load Scenario", list(preset_options.keys()))
if st.sidebar.button("Apply Preset"):
    res = engine_client.get_preset_workload(preset_options[selected_preset])
    if res.get("status") == "success":
        st.session_state.current_workload = res["workload"]
        st.session_state.last_simulation_results = None
        st.session_state.last_comparison_results = None
        st.sidebar.success(f"Loaded '{selected_preset}'!")
        st.rerun()
    else:
        st.sidebar.error("Failed to load preset: " + res.get("message", "Unknown error"))

# Helper function to ensure simulation is run
def get_or_run_simulation():
    if st.session_state.last_simulation_results is None:
        with st.spinner("Executing simulation engine..."):
            res = engine_client.simulate_workload(st.session_state.current_workload)
            if res.get("status") == "success":
                st.session_state.last_simulation_results = res["results"]
            else:
                st.error("Engine Error: " + res.get("message", "Simulation failed"))
    return st.session_state.last_simulation_results

# -----------------------------------------------------------------------------
# PAGE A: OVERVIEW DASHBOARD
# -----------------------------------------------------------------------------
if selected_page == "Page A: Overview Dashboard":
    st.title("📊 System Overview Dashboard")
    st.markdown("Automated priority inversion detection and scheduling telemetry.")
    
    sim_res = get_or_run_simulation()
    
    if sim_res:
        # Top Metrics Bar
        col1, col2, col3, col4, col5 = st.columns(5)
        with col1:
            st.metric("Total Tasks", sim_res.get("total_tasks", len(st.session_state.current_workload["tasks"])))
        with col2:
            st.metric("Shared Mutexes", len(st.session_state.current_workload.get("resources", [])))
        with col3:
            st.metric("Active Protocol", sim_res.get("protocol", "NONE"))
        with col4:
            st.metric("Inversion Scenarios", len(sim_res.get("inversions", [])))
        with col5:
            st.metric("Deadline Misses", sim_res.get("deadline_misses", 0))

        col_a, col_b, col_c = st.columns(3)
        with col_a:
            st.metric("Avg Response Time", f"{sim_res.get('avg_response_time', 0.0):.2f} ticks")
        with col_b:
            st.metric("Worst Response Time", f"{sim_res.get('worst_response_time', 0)} ticks")
        with col_c:
            st.metric("CPU Busy Utilization", f"{sim_res.get('cpu_utilization', 0.0)*100:.1f}%")

        st.markdown("---")
        
        c1, c2 = st.columns([1, 1])
        with c1:
            st.subheader("📋 Active Workload Specification")
            st.markdown(f"**Workload:** `{st.session_state.current_workload.get('name', 'N/A')}`")
            st.markdown(f"*{st.session_state.current_workload.get('description', 'No description')}*")
            
            tasks_data = []
            for t in st.session_state.current_workload.get("tasks", []):
                cs_desc = ", ".join([f"{cs['resource_id']}(len={cs['duration']}@off={cs['offset']})" for cs in t.get("critical_sections", [])]) or "None"
                tasks_data.append({
                    "Task ID": t["id"],
                    "Name": t["name"],
                    "Base Prio": t["priority"],
                    "Release": t["release_time"],
                    "Execution": t["execution_time"],
                    "Deadline": t["deadline"],
                    "Critical Sections": cs_desc
                })
            st.dataframe(pd.DataFrame(tasks_data), use_container_width=True)
            
        with c2:
            st.subheader("⚠️ Most Significant Inversion Event")
            inversions = sim_res.get("inversions", [])
            if inversions:
                # Top blocking inversion
                top_inv = max(inversions, key=lambda x: (x.get("end_time", 0) - x.get("start_time", 0)))
                st.error(f"Inversion on Resource **{top_inv['resource_id']}**")
                st.markdown(f"""
                - **Blocked High-Priority Task:** `{top_inv['high_task_id']}`
                - **Resource Holder (Lower Priority):** `{top_inv['low_task_id']}`
                - **Interfering Mid Tasks:** {", ".join([f"`{t}`" for t in top_inv.get('interfering_tasks', [])]) or "None (Direct blocking)"}
                - **Direct Blocking:** `{top_inv['direct_blocking_time']} ticks`
                - **Interference Delay:** `{top_inv['interference_time']} ticks`
                """)
                st.markdown(f"<div class='causal-box'>{top_inv['causal_chain']}</div>", unsafe_allow_html=True)
            else:
                st.success("✅ No priority inversion detected in this execution.")
                st.info("Tasks did not contend for shared resources in a way that resulted in lower-priority tasks delaying higher-priority tasks via intermediate interference.")

# -----------------------------------------------------------------------------
# PAGE B: TASK & RESOURCE EDITOR
# -----------------------------------------------------------------------------
elif selected_page == "Page B: Task & Resource Editor":
    st.title("⚙️ Task & Resource Configuration Editor")
    st.markdown("Define tasks, execution profiles, critical sections, and shared mutexes.")

    with st.expander("📝 Workload Metadata", expanded=True):
        col1, col2, col3 = st.columns([2, 1, 1])
        with col1:
            st.session_state.current_workload["name"] = st.text_input("Workload Name", st.session_state.current_workload.get("name", ""))
            st.session_state.current_workload["description"] = st.text_input("Description", st.session_state.current_workload.get("description", ""))
        with col2:
            proto = st.selectbox("Scheduling Protocol", ["NONE", "PIP", "PCP"], 
                                 index=["NONE", "PIP", "PCP"].index(st.session_state.current_workload.get("protocol", "NONE")))
            st.session_state.current_workload["protocol"] = proto
        with col3:
            st.session_state.current_workload["simulation_limit"] = st.number_input("Simulation Limit (ticks)", min_value=10, max_value=2000, 
                                                                                    value=st.session_state.current_workload.get("simulation_limit", 100))

    st.subheader("📥 Import Workload (JSON)")
    uploaded_file = st.file_uploader("Upload a JSON Workload File", type=["json"])
    if uploaded_file is not None:
        try:
            loaded_wl = json.load(uploaded_file)
            val_res = engine_client.validate_workload(loaded_wl)
            val = val_res.get("validation", {})
            if val.get("valid"):
                st.session_state.current_workload = loaded_wl
                st.session_state.last_simulation_results = None
                st.session_state.last_comparison_results = None
                st.success("Successfully imported workload!")
                st.rerun()
            else:
                st.error("Uploaded workload is invalid:")
                for err in val.get("errors", []):
                    st.error(f"❌ {err}")
        except Exception as e:
            st.error(f"Failed to parse JSON file: {str(e)}")

    st.subheader("🔒 Shared Mutexes")
    res_list = st.session_state.current_workload.get("resources", [])
    
    col_r1, col_r2 = st.columns([3, 1])
    with col_r1:
        res_ids = [r["id"] for r in res_list]
        st.write(f"Current Resources: {', '.join([f'`{r}`' for r in res_ids]) if res_ids else 'None declared'}")
        # Allow deleting resources
        if res_list:
            del_cols = st.columns(len(res_list))
            for r_idx, r_item in enumerate(res_list):
                with del_cols[r_idx]:
                    if st.button(f"🗑️ Remove {r_item['id']}", key=f"del_res_{r_idx}"):
                        res_list.pop(r_idx)
                        st.session_state.current_workload["resources"] = res_list
                        st.rerun()
    with col_r2:
        new_res_id = st.text_input("New Resource ID (e.g. R2)", "")
        if st.button("Add Resource"):
            if new_res_id and new_res_id not in res_ids:
                res_list.append({"id": new_res_id, "name": f"Resource_{new_res_id}"})
                st.session_state.current_workload["resources"] = res_list
                st.rerun()

    st.subheader("📋 Task Set")
    tasks = st.session_state.current_workload.get("tasks", [])
    
    # Task list editor
    for idx, t in enumerate(tasks):
        with st.expander(f"Task {t['id']} ({t['name']}) - Priority: {t['priority']}", expanded=False):
            c0, c1, c2, c3, c4 = st.columns([1.5, 2, 2, 2, 1.5])
            with c0:
                t["id"] = st.text_input(f"Task ID #{idx}", t["id"], key=f"id_{idx}")
            with c1:
                t["name"] = st.text_input(f"Name #{idx}", t["name"], key=f"name_{idx}")
                t["priority"] = st.number_input(f"Priority (higher=higher) #{idx}", min_value=1, max_value=999, value=t["priority"], key=f"prio_{idx}")
            with c2:
                t["release_time"] = st.number_input(f"Release Time #{idx}", min_value=0, max_value=1000, value=t["release_time"], key=f"rel_{idx}")
                t["execution_time"] = st.number_input(f"Total Execution #{idx}", min_value=1, max_value=1000, value=t["execution_time"], key=f"exec_{idx}")
            with c3:
                t["deadline"] = st.number_input(f"Relative Deadline #{idx}", min_value=1, max_value=1000, value=t["deadline"], key=f"dl_{idx}")
            with c4:
                if st.button(f"🗑️ Delete Task", key=f"del_{idx}"):
                    tasks.pop(idx)
                    st.session_state.current_workload["tasks"] = tasks
                    st.rerun()
            
            # Critical Sections
            st.markdown("**Critical Sections for this Task:**")
            cs_list = t.get("critical_sections", [])
            for cs_idx, cs in enumerate(cs_list):
                csc1, csc2, csc3, csc4 = st.columns([2, 2, 2, 1])
                with csc1:
                    cs_res_opts = res_ids if res_ids else [cs.get("resource_id", "")]
                    cs["resource_id"] = st.selectbox(f"Resource", cs_res_opts, 
                                                     index=cs_res_opts.index(cs["resource_id"]) if cs["resource_id"] in cs_res_opts else 0, 
                                                     key=f"cs_res_{idx}_{cs_idx}")
                with csc2:
                    cs["offset"] = st.number_input(f"Offset (start tick in task)", min_value=0, max_value=1000, value=cs["offset"], key=f"cs_off_{idx}_{cs_idx}")
                with csc3:
                    cs["duration"] = st.number_input(f"Duration", min_value=1, max_value=1000, value=cs["duration"], key=f"cs_dur_{idx}_{cs_idx}")
                with csc4:
                    if st.button("✖", key=f"cs_del_{idx}_{cs_idx}"):
                        cs_list.pop(cs_idx)
                        st.rerun()
            
            if res_ids and st.button(f"➕ Add Critical Section to {t['id']}", key=f"cs_add_{idx}"):
                cs_list.append({"resource_id": res_ids[0], "offset": 0, "duration": 1})
                t["critical_sections"] = cs_list
                st.rerun()

    c_btn1, c_btn2 = st.columns([1, 1])
    with c_btn1:
        if st.button("➕ Create New Task"):
            new_id = f"T{len(tasks)+1}"
            tasks.append({
                "id": new_id,
                "name": f"Task_{new_id}",
                "priority": (len(tasks) + 1) * 2,
                "release_time": 0,
                "execution_time": 5,
                "deadline": 15,
                "critical_sections": []
            })
            st.session_state.current_workload["tasks"] = tasks
            st.rerun()

    with c_btn2:
        if st.button("⚠️ Load Malformed Workload (To Test Error Detection)"):
            st.session_state.current_workload = {
                "name": "Intentionally Malformed Workload",
                "description": "Workload with duplicate IDs, unknown resources, and CS exceeding execution time.",
                "protocol": "NONE",
                "simulation_limit": 50,
                "resources": [{"id": "R1", "name": "Resource_1"}],
                "tasks": [
                    {
                        "id": "T_DUPLICATE",
                        "name": "Task_A",
                        "priority": 5,
                        "release_time": 0,
                        "execution_time": 4,
                        "deadline": 10,
                        "critical_sections": [
                            {"resource_id": "UNKNOWN_MUTEX", "duration": 2, "offset": 0},
                            {"resource_id": "R1", "duration": 4, "offset": 2}
                        ]
                    },
                    {
                        "id": "T_DUPLICATE",
                        "name": "Task_B",
                        "priority": 10,
                        "release_time": 0,
                        "execution_time": 3,
                        "deadline": 10,
                        "critical_sections": []
                    }
                ]
            }
            st.rerun()

    st.markdown("---")
    col_v1, col_v2 = st.columns(2)
    with col_v1:
        if st.button("🔍 Validate Workload"):
            val_res = engine_client.validate_workload(st.session_state.current_workload)
            val = val_res.get("validation", {})
            if val.get("valid"):
                st.success("✅ Workload configuration is valid and ready for simulation!")
            else:
                st.error("❌ Validation Failed! The following errors were identified:")
                for err in val.get("errors", []):
                    st.error(f"• {err}")
    with col_v2:
        if st.button("💾 Apply & Run Simulation"):
            val_res = engine_client.validate_workload(st.session_state.current_workload)
            val = val_res.get("validation", {})
            if not val.get("valid"):
                st.error("Cannot run simulation: workload configuration is invalid. Please resolve the errors above.")
            else:
                st.session_state.last_simulation_results = None
                st.session_state.last_comparison_results = None
                get_or_run_simulation()
                st.success("Simulation complete! Check Page D / E.")

# -----------------------------------------------------------------------------
# PAGE C: SIMULATION WORKSPACE
# -----------------------------------------------------------------------------
elif selected_page == "Page C: Simulation Workspace":
    st.title("🕹️ Simulation Interactive Workspace")
    st.markdown("Execute, inspect, and step through scheduling decisions.")
    
    col1, col2, col3 = st.columns([1, 1, 2])
    with col1:
        proto_choice = st.selectbox("Protocol", ["NONE", "PIP", "PCP"], 
                                    index=["NONE", "PIP", "PCP"].index(st.session_state.current_workload.get("protocol", "NONE")))
        if proto_choice != st.session_state.current_workload.get("protocol"):
            st.session_state.current_workload["protocol"] = proto_choice
            st.session_state.last_simulation_results = None
    with col2:
        if st.button("▶️ Run Entire Simulation"):
            st.session_state.last_simulation_results = None
            get_or_run_simulation()
            st.rerun()
            
    sim_res = get_or_run_simulation()
    
    if sim_res:
        events = sim_res.get("events", [])
        total_time = sim_res.get("total_time", 0)
        
        # Timeline step slider
        step_time = st.slider("Inspect Simulation State at Time Tick", min_value=0, max_value=total_time, value=total_time)
        
        # Filter events up to step_time
        events_upto = [e for e in events if e["time"] <= step_time]
        
        # Determine state at step_time
        # Find which task was running
        current_running = "CPU IDLE"
        for e in events_upto:
            if e["time"] > step_time:
                break
            if e["type"] in ["TASK_START", "TASK_RESUME", "CONTEXT_SWITCH"]:
                current_running = e["task_id"]
            elif e["type"] in ["TASK_PREEMPTED", "TASK_BLOCKED", "TASK_COMPLETED", "PCP_CEILING_BLOCK"]:
                if e["task_id"] == current_running:
                    current_running = "CPU IDLE"
                    
        # State cards
        sc1, sc2, sc3 = st.columns(3)
        with sc1:
            st.markdown(f"### Current Runner: `{current_running}`")
        with sc2:
            st.markdown(f"### Time: `{step_time}` / `{total_time}` ticks")
        with sc3:
            st.markdown(f"### Protocol: `{sim_res.get('protocol')}`")
            
        st.subheader("📜 Chronological Event Log")
        log_df = pd.DataFrame([{
            "Time": e["time"],
            "Event": e["type"],
            "Task": e.get("task_id", ""),
            "Resource": e.get("resource_id", ""),
            "Base Prio": e.get("base_priority", ""),
            "Eff Prio": e.get("effective_priority", ""),
            "Details": e.get("details", "")
        } for e in events_upto])
        st.dataframe(log_df, use_container_width=True, height=300)

# -----------------------------------------------------------------------------
# PAGE D: EXECUTION TIMELINE
# -----------------------------------------------------------------------------
elif selected_page == "Page D: Execution Timeline":
    st.title("📈 Execution Timeline (Gantt Chart)")
    st.markdown("Visual breakdown of task states: Running, Ready, Blocked, and Completed.")
    
    sim_res = get_or_run_simulation()
    if sim_res:
        fig = visualizations.create_gantt_chart(
            sim_res.get("events", []),
            sim_res.get("task_results", []),
            sim_res.get("total_time", 50)
        )
        st.plotly_chart(fig, use_container_width=True)
        
        st.subheader("Task Results Summary")
        tr_df = pd.DataFrame([{
            "Task ID": tr["task_id"],
            "Name": tr["task_name"],
            "Base Priority": tr["base_priority"],
            "Release Time": tr["release_time"],
            "Completion Time": tr["completion_time"],
            "Response Time": tr["response_time"],
            "Waiting Time": tr["waiting_time"],
            "Blocked Time": tr["blocked_time"],
            "Max Block": tr["max_blocking_duration"],
            "Deadline": tr["deadline"],
            "Missed Deadline?": "❌ YES" if tr["deadline_missed"] else "✅ No"
        } for tr in sim_res.get("task_results", [])])
        st.dataframe(tr_df, use_container_width=True)

# -----------------------------------------------------------------------------
# PAGE E: INVERSION ANALYSIS
# -----------------------------------------------------------------------------
elif selected_page == "Page E: Inversion Analysis":
    st.title("🔬 Automated Priority Inversion Diagnosis")
    st.markdown("Causal detection engine tracing resource dependencies and preemption interference.")
    
    sim_res = get_or_run_simulation()
    if sim_res:
        inversions = sim_res.get("inversions", [])
        if not inversions:
            st.success("✅ No Priority Inversion Detected in this workload.")
            st.info("The execution trace shows no high-priority task was delayed by intermediate-priority interference while waiting for a shared resource.")
        else:
            st.warning(f"⚠️ {len(inversions)} Priority Inversion Episode(s) Identified!")
            for idx, inv in enumerate(inversions):
                st.markdown(f"#### Inversion Scenario #{idx + 1}")
                st.markdown(f"<div class='causal-box'>{inv['causal_chain']}</div>", unsafe_allow_html=True)
                
                ic1, ic2, ic3, ic4 = st.columns(4)
                with ic1:
                    st.metric("Blocked High Task", inv["high_task_id"])
                with ic2:
                    st.metric("Resource Owner", inv["low_task_id"])
                with ic3:
                    st.metric("Direct Resource Block", f"{inv['direct_blocking_time']} ticks")
                with ic4:
                    st.metric("Interference Delay", f"{inv['interference_time']} ticks")
                    
                st.markdown(f"**Interfering Tasks (Unrelated Preemptors):** {', '.join([f'`{t}`' for t in inv.get('interfering_tasks', [])]) or 'None'}")
                st.markdown(f"**Resulted in Deadline Miss:** {'❌ YES' if inv.get('caused_deadline_miss') else '✅ No'}")
                st.markdown("---")

            # Visual Mitigation Comparison: Baseline vs PIP
            st.subheader("🛡️ Visual Mitigation Comparison: Baseline vs. PIP Solution")
            st.markdown("Compare the baseline schedule (showing the red blocking inversion) side-by-side with the Priority Inheritance Protocol (PIP) schedule where inheritance resolves the delay.")
            
            pip_workload = dict(st.session_state.current_workload)
            pip_workload["protocol"] = "PIP"
            pip_res_raw = engine_client.simulate_workload(pip_workload)
            if pip_res_raw.get("status") == "success":
                pip_sim = pip_res_raw["results"]
                
                col_g1, col_g2 = st.columns(2)
                with col_g1:
                    st.markdown("##### 🛑 Baseline (Priority Inversion Occurs)")
                    fig_base_e = visualizations.create_gantt_chart(
                        sim_res.get("events", []),
                        sim_res.get("task_results", []),
                        sim_res.get("total_time", 50)
                    )
                    st.plotly_chart(fig_base_e, use_container_width=True, key="e_base_chart")
                with col_g2:
                    st.markdown("##### 🛡️ Priority Inheritance (Inversion Resolved)")
                    fig_pip_e = visualizations.create_gantt_chart(
                        pip_sim.get("events", []),
                        pip_sim.get("task_results", []),
                        pip_sim.get("total_time", 50)
                    )
                    st.plotly_chart(fig_pip_e, use_container_width=True, key="e_pip_chart")

# -----------------------------------------------------------------------------
# PAGE F: PROTOCOL COMPARISON
# -----------------------------------------------------------------------------
elif selected_page == "Page F: Protocol Comparison":
    st.title("⚖️ Resource-Sharing Protocol Comparison")
    st.markdown("Side-by-side evaluation of Baseline (No Inheritance), PIP, and PCP on identical workloads.")
    
    if st.button("🚀 Run Comprehensive Protocol Comparison") or st.session_state.last_comparison_results is None:
        with st.spinner("Simulating Baseline, PIP, and PCP..."):
            res = engine_client.compare_protocols(st.session_state.current_workload)
            if res.get("status") == "success":
                st.session_state.last_comparison_results = res
            else:
                st.error("Comparison Error: " + res.get("message", "Unknown error"))
                
    comp_res = st.session_state.last_comparison_results
    if comp_res:
        baseline = comp_res.get("baseline", {})
        pip = comp_res.get("pip", {})
        pcp = comp_res.get("pcp", {})
        
        # Visual Comparison Bar Chart
        metrics_dict = {
            "Baseline (None)": baseline,
            "Priority Inheritance (PIP)": pip,
            "Priority Ceiling (PCP)": pcp
        }
        fig = visualizations.create_comparison_bar_chart(metrics_dict)
        st.plotly_chart(fig, use_container_width=True)
        
        # Summary Metrics Table
        st.subheader("Key Performance Indicators")
        comp_df = pd.DataFrame({
            "Metric": [
                "Average Response Time",
                "Worst Response Time",
                "Average Waiting Time",
                "Total Blocked Time",
                "Max Blocking Duration",
                "Inversion Time",
                "Deadline Misses",
                "Context Switches",
                "Priority Inheritance Events",
                "Ceiling Blocking Events"
            ],
            "Baseline (None)": [
                f"{baseline.get('avg_response_time', 0.0):.2f}",
                baseline.get('worst_response_time', 0),
                f"{baseline.get('avg_waiting_time', 0.0):.2f}",
                baseline.get('total_blocked_time', 0),
                baseline.get('max_blocking_duration', 0),
                baseline.get('total_priority_inversion_time', 0),
                baseline.get('deadline_misses', 0),
                baseline.get('context_switches', 0),
                0,
                0
            ],
            "PIP": [
                f"{pip.get('avg_response_time', 0.0):.2f}",
                pip.get('worst_response_time', 0),
                f"{pip.get('avg_waiting_time', 0.0):.2f}",
                pip.get('total_blocked_time', 0),
                pip.get('max_blocking_duration', 0),
                pip.get('total_priority_inversion_time', 0),
                pip.get('deadline_misses', 0),
                pip.get('context_switches', 0),
                pip.get('priority_inheritance_events', 0),
                0
            ],
            "PCP": [
                f"{pcp.get('avg_response_time', 0.0):.2f}",
                pcp.get('worst_response_time', 0),
                f"{pcp.get('avg_waiting_time', 0.0):.2f}",
                pcp.get('total_blocked_time', 0),
                pcp.get('max_blocking_duration', 0),
                pcp.get('total_priority_inversion_time', 0),
                pcp.get('deadline_misses', 0),
                pcp.get('context_switches', 0),
                pcp.get('priority_inheritance_events', 0),
                pcp.get('protocol_blocking_events', 0)
            ]
        })
        st.dataframe(comp_df, use_container_width=True)
        
        # Comparative Gantt Timelines for all 3 protocols
        st.subheader("📅 Comparative Protocol Execution Timelines")
        st.markdown("Visually inspect and compare the exact execution Gantt schedule under each protocol.")
        
        timeline_tab1, timeline_tab2, timeline_tab3, timeline_tab4 = st.tabs([
            "🛡️ Priority Inheritance (PIP)",
            "🛑 Baseline (No Inheritance)",
            "🔒 Priority Ceiling (PCP)",
            "📊 All 3 Stacked View"
        ])
        
        fig_pip_tab = visualizations.create_gantt_chart(
            pip.get("events", []),
            pip.get("task_results", []),
            pip.get("total_time", 50)
        )
        fig_base_tab = visualizations.create_gantt_chart(
            baseline.get("events", []),
            baseline.get("task_results", []),
            baseline.get("total_time", 50)
        )
        fig_pcp_tab = visualizations.create_gantt_chart(
            pcp.get("events", []),
            pcp.get("task_results", []),
            pcp.get("total_time", 50)
        )
        
        with timeline_tab1:
            st.markdown("**Priority Inheritance Protocol (PIP):** Prevents intermediate preemption of the resource holder.")
            st.plotly_chart(fig_pip_tab, use_container_width=True, key="f_gantt_pip")
            
        with timeline_tab2:
            st.markdown("**Baseline (No Inheritance):** Demonstrates uncontrolled priority inversion delays.")
            st.plotly_chart(fig_base_tab, use_container_width=True, key="f_gantt_base")
            
        with timeline_tab3:
            st.markdown("**Priority Ceiling Protocol (PCP):** Dynamically bounds blocking and prevents chained deadlocks.")
            st.plotly_chart(fig_pcp_tab, use_container_width=True, key="f_gantt_pcp")
            
        with timeline_tab4:
            st.markdown("#### 1. Baseline (None)")
            st.plotly_chart(fig_base_tab, use_container_width=True, key="f_stacked_base")
            st.markdown("#### 2. Priority Inheritance Protocol (PIP)")
            st.plotly_chart(fig_pip_tab, use_container_width=True, key="f_stacked_pip")
            st.markdown("#### 3. Priority Ceiling Protocol (PCP)")
            st.plotly_chart(fig_pcp_tab, use_container_width=True, key="f_stacked_pcp")

        # Recommendations Section
        st.subheader("💡 Evidence-Based Recommendations")
        recs = comp_res.get("recommendations", [])
        for r in recs:
            st.markdown(f"""
            <div class='rec-box'>
                <b>Problem:</b> {r['problem']}<br>
                <b>Evidence:</b> {r['evidence']}<br>
                <b>Recommendation:</b> <code>{r['suggestion']}</code><br>
                <b>Expected Benefit:</b> {r['benefit']}<br>
                <b>Trade-off:</b> {r['tradeoff']}
            </div>
            """, unsafe_allow_html=True)

# -----------------------------------------------------------------------------
# PAGE G: EXPERIMENT LAB
# -----------------------------------------------------------------------------
elif selected_page == "Page G: Experiment Lab":
    st.title("🧪 Batch Experimentation Lab")
    st.markdown("Generate synthetic workloads with controlled parameters and run batch comparisons.")
    
    with st.expander("⚙️ Generator Settings", expanded=True):
        c1, c2, c3, c4 = st.columns(4)
        with c1:
            num_tasks = st.slider("Tasks per workload", 3, 10, 4)
            num_resources = st.slider("Shared Resources", 1, 5, 2)
        with c2:
            num_workloads = st.slider("Workloads to generate", 5, 50, 10)
            seed = st.number_input("Random Seed", value=42)
        with c3:
            sharing_prob = st.slider("Resource Sharing Prob", 0.1, 1.0, 0.6)
            deadline_tight = st.slider("Deadline Tightness", 1.2, 3.0, 1.8)
        with c4:
            min_exec = st.number_input("Min Exec Time", value=3)
            max_exec = st.number_input("Max Exec Time", value=12)

    if st.button("🚀 Execute Batch Experiment"):
        config = {
            "num_tasks": num_tasks,
            "num_resources": num_resources,
            "num_workloads": num_workloads,
            "seed": int(seed),
            "resource_sharing_prob": sharing_prob,
            "deadline_tightness": deadline_tight,
            "min_execution": int(min_exec),
            "max_execution": int(max_exec),
            "min_cs_length": 1,
            "max_cs_length": 4,
            "max_release_time": 10,
            "min_priority": 1,
            "max_priority": 20
        }
        with st.spinner(f"Running batch simulation for {num_workloads} synthetic workloads..."):
            batch_res = engine_client.run_batch_experiment(config)
            if batch_res.get("status") == "success":
                agg = batch_res.get("aggregate", {})
                st.success("Batch experiment complete!")
                
                # Aggregate Cards
                a1, a2, a3, a4 = st.columns(4)
                with a1:
                    st.metric("Workloads with Inversion", f"{agg.get('inversion_percentage', 0.0):.1f}%")
                with a2:
                    st.metric("Total Inversions (Base)", agg.get("total_inversions_baseline", 0))
                with a3:
                    st.metric("Misses: Base vs PIP vs PCP", f"{agg.get('total_deadline_misses_baseline')} / {agg.get('total_deadline_misses_pip')} / {agg.get('total_deadline_misses_pcp')}")
                with a4:
                    st.metric("Avg Resp: Base vs PIP vs PCP", f"{agg.get('avg_response_baseline', 0):.1f} / {agg.get('avg_response_pip', 0):.1f} / {agg.get('avg_response_pcp', 0):.1f}")
                
                # Per-workload details
                st.subheader("Batch Workload Summaries")
                wl_res = batch_res.get("workload_results", [])
                st.dataframe(pd.DataFrame(wl_res), use_container_width=True)
            else:
                st.error("Batch experiment failed: " + batch_res.get("message", "Unknown error"))

# -----------------------------------------------------------------------------
# PAGE H: REPORTS & EXPORTS
# -----------------------------------------------------------------------------
elif selected_page == "Page H: Reports & Exports":
    st.title("📑 Reports & Data Exports")
    st.markdown("Export simulation traces, task configurations, and generate academic summary reports.")
    
    sim_res = get_or_run_simulation()
    
    col1, col2 = st.columns(2)
    with col1:
        st.subheader("Export Task Configuration")
        workload_json = json.dumps(st.session_state.current_workload, indent=2)
        st.download_button(
            label="💾 Download Workload (JSON)",
            data=workload_json,
            file_name=f"{st.session_state.current_workload.get('name', 'workload').replace(' ', '_')}.json",
            mime="application/json"
        )
        
    with col2:
        st.subheader("Export Simulation Trace")
        if sim_res:
            trace_json = json.dumps(sim_res, indent=2)
            st.download_button(
                label="💾 Download Full Trace (JSON)",
                data=trace_json,
                file_name="simulation_trace.json",
                mime="application/json"
            )
            
            # Events CSV
            events_df = pd.DataFrame(sim_res.get("events", []))
            st.download_button(
                label="💾 Download Events (CSV)",
                data=events_df.to_csv(index=False),
                file_name="simulation_events.csv",
                mime="text/csv"
            )
            
    st.markdown("---")
    st.subheader("📄 Academic Evaluation Report")
    report_text = f"""# PriorityGuard Research & Evaluation Report

## 1. Research Question & Objective
How do resource-sharing protocols (Priority Inheritance Protocol vs. Priority Ceiling Protocol) 
mitigate priority inversion and reduce worst-case task blocking durations in real-time preemptive fixed-priority scheduling?

## 2. Experimental Methodology
- Discrete-event single-core simulation engine adhering strictly to real-time operating systems semantics.
- Evaluation on Workload: {st.session_state.current_workload.get('name')}
- Active Protocol: {st.session_state.current_workload.get('protocol')}
- Preemption model: Preemptive fixed-priority scheduling with deterministic tie-breaking.

## 3. Measured Simulation Results
- Total Tasks: {len(st.session_state.current_workload.get('tasks', []))}
- Priority Inversion Episodes: {len(sim_res.get('inversions', [])) if sim_res else 0}
- Total Blocked Duration: {sim_res.get('total_blocked_time', 0) if sim_res else 0} ticks
- Average Task Response Time: {sim_res.get('avg_response_time', 0.0) if sim_res else 0:.2f} ticks
- Deadline Misses: {sim_res.get('deadline_misses', 0) if sim_res else 0}

## 4. Discussion & Limitations
- Single-core assumption: Multi-core multiprocessor priority inversion (e.g., MrsP, FMLP) is out of scope.
- Measurements represent observed simulation trajectories, not analytical worst-case upper bounds.
"""
    st.text_area("Generated Summary Report", report_text, height=350)
    st.download_button("💾 Download Academic Report (Markdown)", report_text, file_name="PriorityGuard_Report.md", mime="text/markdown")

