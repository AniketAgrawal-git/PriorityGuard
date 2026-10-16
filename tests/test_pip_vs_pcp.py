import sys
sys.path.append('frontend')
import engine_client

wl = {
    'name': 'PCP vs PIP Divergence Test',
    'description': 'Demonstrates PCP ceiling blocking on a free resource vs PIP greedy acquisition',
    'protocol': 'PIP',
    'simulation_limit': 50,
    'resources': [
        {'id': 'R1', 'name': 'Resource_1'},
        {'id': 'R2', 'name': 'Resource_2'}
    ],
    'tasks': [
        {'id': 'TL', 'name': 'Task_L', 'priority': 1, 'release_time': 0, 'execution_time': 8, 'deadline': 30, 'critical_sections': [{'resource_id': 'R1', 'offset': 1, 'duration': 5}]},
        {'id': 'TM', 'name': 'Task_M', 'priority': 6, 'release_time': 2, 'execution_time': 5, 'deadline': 25, 'critical_sections': [{'resource_id': 'R2', 'offset': 0, 'duration': 3}]},
        {'id': 'TH', 'name': 'Task_H', 'priority': 10, 'release_time': 4, 'execution_time': 4, 'deadline': 15, 'critical_sections': [{'resource_id': 'R1', 'offset': 0, 'duration': 2}]}
    ]
}

sim_pip = engine_client.simulate_workload({**wl, 'protocol': 'PIP'})['results']
sim_pcp = engine_client.simulate_workload({**wl, 'protocol': 'PCP'})['results']

print("=== PIP SIGNIFICANT EVENTS ===")
for e in sim_pip['events']:
    if e['type'] in ['TASK_BLOCKED', 'PCP_CEILING_BLOCK', 'MUTEX_ACQUIRE', 'PRIORITY_INHERIT']:
        print(f"  t={e['time']}: {e['type']} by {e['task_id']} on {e.get('resource_id', '')} (prio={e.get('effective_priority')}) - {e.get('details')}")

print("\n=== PCP SIGNIFICANT EVENTS ===")
for e in sim_pcp['events']:
    if e['type'] in ['TASK_BLOCKED', 'PCP_CEILING_BLOCK', 'MUTEX_ACQUIRE', 'PRIORITY_INHERIT']:
        print(f"  t={e['time']}: {e['type']} by {e['task_id']} on {e.get('resource_id', '')} (prio={e.get('effective_priority')}) - {e.get('details')}")

print("\n=== RESPONSE TIMES ===")
print("PIP Response Times:", {t['task_id']: t['response_time'] for t in sim_pip['task_results']})
print("PCP Response Times:", {t['task_id']: t['response_time'] for t in sim_pcp['task_results']})
print("PCP Ceiling Blocking Events:", sim_pcp.get('protocol_blocking_events'))
