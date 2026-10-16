import plotly.graph_objects as go
import plotly.express as px
from typing import List, Dict, Any

def create_gantt_chart(events: List[Dict[str, Any]], task_results: List[Dict[str, Any]], total_time: int) -> go.Figure:
    """
    Build an interactive Gantt chart of CPU execution, task states, and events.
    """
    fig = go.Figure()
    
    # Track task names and IDs
    task_order = [tr["task_id"] for tr in task_results]
    # Reverse so highest priority / first appears at the top
    task_order_reversed = list(reversed(task_order))
    
    # Colors for states
    state_colors = {
        "RUNNING": "#00CC96",      # Emerald Green
        "READY": "#FFA15A",        # Orange/Amber
        "BLOCKED": "#EF553B",      # Bright Red
        "COMPLETED": "#636EFA"     # Slate Blue
    }
    
    # Reconstruct state tick-by-tick using chronological events
    events_by_time = {}
    for e in events:
        events_by_time.setdefault(e["time"], []).append(e)

    tick_states = {tid: [] for tid in task_order}
    state_map = {tid: "NOT_RELEASED" for tid in task_order}

    for t in range(total_time):
        if t in events_by_time:
            for e in events_by_time[t]:
                etype = e["type"]
                tid = e.get("task_id")
                if not tid or tid not in state_map:
                    continue
                if etype == "TASK_RELEASE":
                    state_map[tid] = "READY"
                elif etype in ("TASK_START", "TASK_RESUME"):
                    state_map[tid] = "RUNNING"
                elif etype == "TASK_PREEMPTED":
                    state_map[tid] = "READY"
                elif etype in ("TASK_BLOCKED", "PCP_CEILING_BLOCK"):
                    state_map[tid] = "BLOCKED"
                elif etype == "TASK_UNBLOCKED":
                    state_map[tid] = "READY"
                elif etype == "TASK_COMPLETED":
                    state_map[tid] = "COMPLETED"

        for tid in task_order:
            tick_states[tid].append(state_map[tid])

    # Convert tick states into contiguous intervals
    task_intervals = {tid: [] for tid in task_order}
    for tid in task_order:
        tlist = tick_states[tid]
        if not tlist:
            continue
        cur_st = tlist[0]
        cur_start = 0
        for i in range(1, len(tlist)):
            if tlist[i] != cur_st:
                if cur_st not in ("NOT_RELEASED", "COMPLETED"):
                    task_intervals[tid].append({"state": cur_st, "start": cur_start, "end": i})
                cur_st = tlist[i]
                cur_start = i
        if cur_st not in ("NOT_RELEASED", "COMPLETED"):
            task_intervals[tid].append({"state": cur_st, "start": cur_start, "end": len(tlist)})
            
    # Draw bars
    added_legend = set()
    for tid in task_order:
        intervals = task_intervals[tid]
        for iv in intervals:
            state = iv["state"]
            color = state_colors.get(state, "#AB63FA")
            show_leg = state not in added_legend
            added_legend.add(state)
            
            fig.add_trace(go.Bar(
                name=state,
                y=[tid],
                x=[iv["end"] - iv["start"]],
                base=[iv["start"]],
                orientation='h',
                marker=dict(color=color, line=dict(color='#2A2E39', width=1)),
                hovertext=f"Task: {tid}<br>State: {state}<br>Start: {iv['start']}<br>End: {iv['end']}<br>Duration: {iv['end'] - iv['start']}",
                hoverinfo="text",
                showlegend=show_leg,
                legendgroup=state
            ))

    # Add markers for significant milestones
    for e in events:
        etype = e["type"]
        tid = e.get("task_id")
        t = e["time"]
        prio = e.get("effective_priority", e.get("base_priority", 0))
        
        if etype == "PRIORITY_INHERIT":
            fig.add_trace(go.Scatter(
                x=[t], y=[tid],
                mode='markers',
                marker=dict(symbol='triangle-up', size=12, color='#AB63FA'),
                name="Priority Inherit",
                legendgroup="events",
                showlegend="Priority Inherit" not in added_legend,
                hovertext=f"Time {t}: {e['details']}",
                hoverinfo="text"
            ))
            added_legend.add("Priority Inherit")
            
        elif etype == "PRIORITY_RESTORE":
            fig.add_trace(go.Scatter(
                x=[t], y=[tid],
                mode='markers',
                marker=dict(symbol='triangle-down', size=12, color='#19D3F3'),
                name="Priority Restore",
                legendgroup="events",
                showlegend="Priority Restore" not in added_legend,
                hovertext=f"Time {t}: {e['details']}",
                hoverinfo="text"
            ))
            added_legend.add("Priority Restore")
            
        elif etype == "DEADLINE_MISS":
            fig.add_trace(go.Scatter(
                x=[t], y=[tid],
                mode='markers',
                marker=dict(symbol='x', size=14, color='#FF0000', line=dict(width=2)),
                name="Deadline Miss",
                legendgroup="events",
                showlegend="Deadline Miss" not in added_legend,
                hovertext=f"DEADLINE MISSED at {t}: Task {tid}",
                hoverinfo="text"
            ))
            added_legend.add("Deadline Miss")
            
        elif etype in ["MUTEX_ACQUIRE", "MUTEX_RELEASE"]:
            symbol = 'square' if etype == "MUTEX_ACQUIRE" else 'circle-open'
            fig.add_trace(go.Scatter(
                x=[t], y=[tid],
                mode='markers',
                marker=dict(symbol=symbol, size=9, color='#FECB52'),
                name="Mutex Event",
                legendgroup="events",
                showlegend="Mutex Event" not in added_legend,
                hovertext=f"Time {t}: {e['details']}",
                hoverinfo="text"
            ))
            added_legend.add("Mutex Event")

    # Add vertical lines for deadlines
    for tr in task_results:
        tid = tr["task_id"]
        dl = tr["deadline"]
        fig.add_trace(go.Scatter(
            x=[dl, dl],
            y=[tid, tid],
            mode='lines',
            line=dict(color='rgba(255, 99, 71, 0.8)', width=3, dash='dash'),
            name="Deadline",
            legendgroup="deadline",
            showlegend="Deadline" not in added_legend,
            hovertext=f"Deadline for {tid}: {dl}",
            hoverinfo="text"
        ))
        added_legend.add("Deadline")

    fig.update_layout(
        title="Interactive Task Execution Gantt Timeline",
        barmode='overlay',
        xaxis=dict(
            title="Simulation Time (ticks)",
            tickmode='linear',
            tick0=0,
            dtick=max(1, total_time // 20),
            gridcolor='#333A42',
            showgrid=True
        ),
        yaxis=dict(
            title="Tasks",
            categoryorder='array',
            categoryarray=task_order_reversed,
            gridcolor='#333A42'
        ),
        plot_bgcolor='#1E222A',
        paper_bgcolor='#1E222A',
        font=dict(color='#E1E4EA', family="Segoe UI, sans-serif"),
        height=350 + (len(task_order) * 45),
        margin=dict(l=60, r=40, t=50, b=50),
        legend=dict(
            orientation="h",
            yanchor="bottom",
            y=1.02,
            xanchor="right",
            x=1
        )
    )
    
    return fig

def create_comparison_bar_chart(metrics: Dict[str, Dict[str, Any]]) -> go.Figure:
    """Create multi-protocol comparison bar chart."""
    protocols = list(metrics.keys())
    
    fig = go.Figure()
    
    # Metrics to compare
    metric_keys = [
        ("avg_response_time", "Avg Response Time", "#636EFA"),
        ("total_blocked_time", "Total Blocked Time", "#EF553B"),
        ("total_priority_inversion_time", "Inversion Duration", "#FFA15A"),
        ("deadline_misses", "Deadline Misses", "#FF6692")
    ]
    
    for key, label, color in metric_keys:
        vals = [metrics[p].get(key, 0) for p in protocols]
        fig.add_trace(go.Bar(
            name=label,
            x=protocols,
            y=vals,
            marker_color=color,
            text=[f"{v:.1f}" if isinstance(v, float) else str(v) for v in vals],
            textposition='auto'
        ))
        
    fig.update_layout(
        title="Protocol Performance Comparison",
        barmode='group',
        plot_bgcolor='#1E222A',
        paper_bgcolor='#1E222A',
        font=dict(color='#E1E4EA', family="Segoe UI, sans-serif"),
        xaxis=dict(gridcolor='#333A42'),
        yaxis=dict(gridcolor='#333A42', title="Measured Value"),
        margin=dict(l=40, r=40, t=50, b=50),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )
    return fig

