"""
PriorityGuard - State Inspector & Trace Analysis Module
Reconstructs exact micro-states (tasks, effective priorities, mutex ownership, ready queue,
blocked queue, and scheduling decisions) at any discrete simulation step, and performs
divergence analysis between execution traces.
"""

from typing import Dict, Any, List, Optional

def compute_priority_ceilings(workload: Dict[str, Any]) -> Dict[str, int]:
    """Compute the priority ceiling for each resource defined in the workload."""
    ceilings = {}
    for res in workload.get("resources", []):
        rid = res["id"]
        max_prio = 0
        for task in workload.get("tasks", []):
            for cs in task.get("critical_sections", []):
                if cs.get("resource_id") == rid:
                    if task.get("priority", 0) > max_prio:
                        max_prio = task.get("priority", 0)
        ceilings[rid] = max_prio
    return ceilings

def compute_system_snapshot(
    workload: Dict[str, Any],
    events: List[Dict[str, Any]],
    target_event_idx: int
) -> Dict[str, Any]:
    """
    Reconstruct the full system state up to target_event_idx (inclusive).
    Returns task states, dynamic effective priorities, mutex ownership, queues,
    and scheduling decision rationale.
    """
    if not events:
        return {
            "time": 0,
            "event_idx": 0,
            "last_event": None,
            "current_runner": "CPU IDLE",
            "task_states": {},
            "mutex_states": {},
            "ready_queue": [],
            "blocked_queue": [],
            "scheduling_decision": "No simulation events to inspect."
        }

    target_idx = max(0, min(target_event_idx, len(events) - 1))
    ceilings = compute_priority_ceilings(workload)
    
    # Initialize tasks
    tasks_info = {}
    for t in workload.get("tasks", []):
        tid = t["id"]
        tasks_info[tid] = {
            "id": tid,
            "name": t.get("name", tid),
            "base_priority": t.get("priority", 0),
            "effective_priority": t.get("priority", 0),
            "release_time": t.get("release_time", 0),
            "execution_time": t.get("execution_time", 0),
            "deadline": t.get("release_time", 0) + t.get("deadline", 0),
            "state": "NOT_RELEASED",
            "executed_time": 0,
            "holding_resources": [],
            "blocked_on_resource": None,
            "is_deadline_missed": False
        }

    # Initialize mutexes
    mutex_info = {}
    for r in workload.get("resources", []):
        rid = r["id"]
        mutex_info[rid] = {
            "id": rid,
            "name": r.get("name", rid),
            "is_locked": False,
            "owner": None,
            "wait_queue": [],
            "ceiling": ceilings.get(rid, 0)
        }

    current_runner = "CPU IDLE"
    last_event = None
    scheduling_notes = []

    # Replay events up to target_idx
    for idx in range(target_idx + 1):
        e = events[idx]
        last_event = e
        t = e.get("time", 0)
        etype = e.get("type", "")
        tid = e.get("task_id", "")
        rid = e.get("resource_id", "")
        eff_prio = e.get("effective_priority")
        details = e.get("details", "")

        # Update dynamic effective priority if recorded in event
        if tid and tid in tasks_info and eff_prio is not None:
            tasks_info[tid]["effective_priority"] = eff_prio

        # Check deadline misses
        for task_id, tinfo in tasks_info.items():
            if t > tinfo["deadline"] and tinfo["state"] != "COMPLETED":
                tinfo["is_deadline_missed"] = True

        if etype == "TASK_RELEASE":
            if tid in tasks_info:
                tasks_info[tid]["state"] = "READY"
                if idx == target_idx:
                    scheduling_notes.append(f"Task `{tid}` arrived at time {t} and entered the Ready Queue with priority {tasks_info[tid]['effective_priority']}.")

        elif etype in ("TASK_START", "TASK_RESUME"):
            if tid in tasks_info:
                tasks_info[tid]["state"] = "RUNNING"
                current_runner = tid
                if idx == target_idx:
                    scheduling_notes.append(f"Dispatcher selected `{tid}` (effective priority {tasks_info[tid]['effective_priority']}) as the highest-priority eligible ready task.")

        elif etype == "TASK_PREEMPTED":
            if tid in tasks_info:
                tasks_info[tid]["state"] = "READY"
                if current_runner == tid:
                    current_runner = "CPU IDLE"
                if idx == target_idx:
                    scheduling_notes.append(f"Task `{tid}` was preempted by a higher effective priority task.")

        elif etype in ("TASK_BLOCKED", "PCP_CEILING_BLOCK"):
            if tid in tasks_info:
                tasks_info[tid]["state"] = "BLOCKED"
                tasks_info[tid]["blocked_on_resource"] = rid
                if current_runner == tid:
                    current_runner = "CPU IDLE"
                if rid and rid in mutex_info:
                    if tid not in mutex_info[rid]["wait_queue"]:
                        mutex_info[rid]["wait_queue"].append(tid)
                if idx == target_idx:
                    if etype == "PCP_CEILING_BLOCK":
                        scheduling_notes.append(f"PCP Ceiling Protocol Block: `{tid}` attempted to lock `{rid}`, but active system ceiling prevented acquisition. `{tid}` placed in wait queue.")
                    else:
                        owner = mutex_info[rid]["owner"] if rid in mutex_info else "another task"
                        scheduling_notes.append(f"Task `{tid}` requested mutex `{rid}` which is locked by `{owner}`. `{tid}` entered BLOCKED state.")

        elif etype == "TASK_UNBLOCKED":
            if tid in tasks_info:
                tasks_info[tid]["state"] = "READY"
                tasks_info[tid]["blocked_on_resource"] = None
                if rid and rid in mutex_info:
                    if tid in mutex_info[rid]["wait_queue"]:
                        mutex_info[rid]["wait_queue"].remove(tid)
                if idx == target_idx:
                    scheduling_notes.append(f"Task `{tid}` unblocked after mutex `{rid}` was released.")

        elif etype == "TASK_COMPLETED":
            if tid in tasks_info:
                tasks_info[tid]["state"] = "COMPLETED"
                if current_runner == tid:
                    current_runner = "CPU IDLE"
                if idx == target_idx:
                    scheduling_notes.append(f"Task `{tid}` completed all execution demands.")

        elif etype == "MUTEX_ACQUIRE":
            if rid and rid in mutex_info:
                mutex_info[rid]["is_locked"] = True
                mutex_info[rid]["owner"] = tid
            if tid and tid in tasks_info:
                if rid not in tasks_info[tid]["holding_resources"]:
                    tasks_info[tid]["holding_resources"].append(rid)
            if idx == target_idx:
                scheduling_notes.append(f"Task `{tid}` acquired mutex `{rid}`.")

        elif etype == "MUTEX_RELEASE":
            if rid and rid in mutex_info:
                mutex_info[rid]["is_locked"] = False
                mutex_info[rid]["owner"] = None
            if tid and tid in tasks_info:
                if rid in tasks_info[tid]["holding_resources"]:
                    tasks_info[tid]["holding_resources"].remove(rid)
            if idx == target_idx:
                scheduling_notes.append(f"Task `{tid}` released mutex `{rid}`.")

        elif etype == "PRIORITY_INHERIT":
            if tid in tasks_info and eff_prio is not None:
                tasks_info[tid]["effective_priority"] = eff_prio
            if idx == target_idx:
                scheduling_notes.append(f"Priority Inheritance: `{tid}` boosted to effective priority {eff_prio} to prevent priority inversion.")

        elif etype == "PRIORITY_RESTORE":
            if tid in tasks_info and eff_prio is not None:
                tasks_info[tid]["effective_priority"] = eff_prio
            if idx == target_idx:
                scheduling_notes.append(f"Priority Restored: `{tid}` returned to priority {eff_prio} after releasing contested mutex.")

        elif etype == "DEADLINE_MISS":
            if tid in tasks_info:
                tasks_info[tid]["is_deadline_missed"] = True
            if idx == target_idx:
                scheduling_notes.append(f"⚠️ Task `{tid}` missed its deadline at time {t}!")

        elif etype == "SIMULATION_END":
            if idx == target_idx:
                scheduling_notes.append(f"Simulation execution reached conclusion limit at time {t}.")

    # Build queues
    ready_queue = []
    blocked_queue = []
    for tid, tinfo in tasks_info.items():
        if tinfo["state"] == "READY":
            ready_queue.append({
                "task_id": tid,
                "name": tinfo["name"],
                "base_prio": tinfo["base_priority"],
                "eff_prio": tinfo["effective_priority"],
                "release_time": tinfo["release_time"]
            })
        elif tinfo["state"] == "BLOCKED":
            blocked_queue.append({
                "task_id": tid,
                "name": tinfo["name"],
                "base_prio": tinfo["base_priority"],
                "eff_prio": tinfo["effective_priority"],
                "blocked_on": tinfo["blocked_on_resource"],
                "holder": mutex_info.get(tinfo["blocked_on_resource"], {}).get("owner", "Unknown") if tinfo["blocked_on_resource"] else "N/A"
            })

    # Sort ready queue by effective priority descending, then earliest release, then task ID
    ready_queue.sort(key=lambda x: (-x["eff_prio"], x["release_time"], x["task_id"]))

    current_time = last_event.get("time", 0) if last_event else 0
    decision_text = " ".join(scheduling_notes) if scheduling_notes else f"State at time {current_time}."

    return {
        "time": current_time,
        "event_idx": target_idx,
        "total_events": len(events),
        "last_event": last_event,
        "current_runner": current_runner,
        "task_states": tasks_info,
        "mutex_states": mutex_info,
        "ready_queue": ready_queue,
        "blocked_queue": blocked_queue,
        "scheduling_decision": decision_text
    }

def compare_execution_traces(
    sim_a: Dict[str, Any],
    sim_b: Dict[str, Any],
    proto_a_name: str = "Baseline",
    proto_b_name: str = "PIP"
) -> Dict[str, Any]:
    """
    Compare two execution traces tick-by-tick and identify divergence points.
    """
    events_a = sim_a.get("events", [])
    events_b = sim_b.get("events", [])
    total_time_a = sim_a.get("total_time", 0)
    total_time_b = sim_b.get("total_time", 0)
    max_time = max(total_time_a, total_time_b, 1)

    # Reconstruct CPU runner tick-by-tick for both
    def get_runner_schedule(events: List[Dict[str, Any]], max_t: int) -> List[str]:
        sched = ["IDLE"] * max_t
        cur = "IDLE"
        events_by_t = {}
        for e in events:
            events_by_t.setdefault(e.get("time", 0), []).append(e)
            
        for t in range(max_t):
            if t in events_by_t:
                for e in events_by_t[t]:
                    etype = e.get("type")
                    tid = e.get("task_id")
                    if etype in ("TASK_START", "TASK_RESUME"):
                        cur = tid
                    elif etype in ("TASK_PREEMPTED", "TASK_BLOCKED", "TASK_COMPLETED", "PCP_CEILING_BLOCK"):
                        if cur == tid:
                            cur = "IDLE"
            sched[t] = cur
        return sched

    sched_a = get_runner_schedule(events_a, max_time)
    sched_b = get_runner_schedule(events_b, max_time)

    # Find divergence
    divergence_time = None
    divergence_explanation = None

    for t in range(max_time):
        if sched_a[t] != sched_b[t]:
            divergence_time = t
            runner_a = sched_a[t]
            runner_b = sched_b[t]
            divergence_explanation = (
                f"At time t={t}, execution schedules diverge: "
                f"Under {proto_a_name}, task `{runner_a}` executes on the CPU, whereas under "
                f"{proto_b_name}, task `{runner_b}` executes on the CPU."
            )
            break

    if divergence_time is None:
        divergence_explanation = (
            f"Schedules under {proto_a_name} and {proto_b_name} execute identical task sequences "
            f"across all {max_time} simulation ticks."
        )

    # Compare task metrics
    tasks_a = {tr["task_id"]: tr for tr in sim_a.get("task_results", [])}
    tasks_b = {tr["task_id"]: tr for tr in sim_b.get("task_results", [])}
    all_task_ids = sorted(list(set(tasks_a.keys()) | set(tasks_b.keys())))

    task_diffs = []
    for tid in all_task_ids:
        ta = tasks_a.get(tid, {})
        tb = tasks_b.get(tid, {})
        task_diffs.append({
            "task_id": tid,
            "name": ta.get("task_name") or tb.get("task_name", tid),
            "base_priority": ta.get("base_priority") or tb.get("base_priority", 0),
            "resp_a": ta.get("response_time", "N/A"),
            "resp_b": tb.get("response_time", "N/A"),
            "resp_diff": (tb.get("response_time", 0) - ta.get("response_time", 0)) if ("response_time" in ta and "response_time" in tb) else "N/A",
            "blocked_a": ta.get("blocked_time", 0),
            "blocked_b": tb.get("blocked_time", 0),
            "miss_a": ta.get("deadline_missed", False),
            "miss_b": tb.get("deadline_missed", False),
        })

    return {
        "proto_a": proto_a_name,
        "proto_b": proto_b_name,
        "max_time": max_time,
        "sched_a": sched_a,
        "sched_b": sched_b,
        "divergence_time": divergence_time,
        "divergence_explanation": divergence_explanation,
        "task_diffs": task_diffs,
        "inversions_a": len(sim_a.get("inversions", [])),
        "inversions_b": len(sim_b.get("inversions", [])),
        "deadlines_a": sim_a.get("deadline_misses", 0),
        "deadlines_b": sim_b.get("deadline_misses", 0),
        "total_blocked_a": sim_a.get("total_blocked_time", 0),
        "total_blocked_b": sim_b.get("total_blocked_time", 0),
    }

