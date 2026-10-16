# PriorityGuard Architecture & Theory Guide

## 1. Scheduling Model & Assumptions
- **Preemptive Fixed-Priority (FP)**: The scheduler continually evaluates the ready queue and allocates the single CPU core to the eligible task with the highest effective priority.
- **Deterministic Tie-Breaking**: When multiple tasks possess identical effective priorities, the task with the earliest arrival/release time is preferred; remaining ties are broken by lexicographical task identifier (`id`).
- **Resource Ownership**: Mutexes are mutually exclusive and strictly binary. A task attempting to acquire a locked mutex transitions immediately to the `BLOCKED` state and yields the CPU without consuming execution cycles.

## 2. Resource-Sharing Protocols

### Baseline (NONE)
- Tasks block directly when requesting a resource held by another task.
- Unrelated intermediate-priority tasks can preempt the lower-priority holder, causing unbound priority inversion for the blocked higher-priority task.

### Priority Inheritance Protocol (PIP)
- When a task $T_H$ blocks on a resource held by $T_L$ ($Priority(T_H) > Priority(T_L)$), $T_L$ dynamically inherits $T_H$'s effective priority:
  $$\text{EffectivePriority}(T_L) \leftarrow \max(\text{BasePriority}(T_L), \text{EffectivePriority}(T_H))$$
- Intermediate tasks $T_M$ ($Priority(T_L) < Priority(T_M) < Priority(T_H)$) cannot preempt $T_L$ while it holds the resource.
- **Transitive Inheritance**: If $T_L$ is blocked on another resource held by $T_{LL}$, priority boosts propagate recursively down the dependency chain.
- Priority is immediately restored to base or the highest remaining blocked waiter upon resource release.

### Priority Ceiling Protocol (PCP)
- Each shared resource $R_i$ is statically assigned a ceiling equal to the highest priority of any task that may lock it:
  $$\Pi(R_i) = \max \{ Priority(T) \mid T \text{ uses } R_i \}$$
- **System Ceiling**: At any instant, $\Pi^*$ is the maximum ceiling among all currently locked resources.
- **Admission Rule**: A task $T$ can only lock a resource $R$ if $R$ is free AND:
  $$Priority(T) > \Pi^*_{\text{other}}$$
  where $\Pi^*_{\text{other}}$ is the maximum ceiling of mutexes locked by tasks other than $T$.
- If blocked by the ceiling rule, $T$ blocks, and the holder of the resource establishing $\Pi^*$ inherits $T$'s priority.
- Guarantees absence of deadlock and at most one critical section of blocking for any task.

## 3. Inversion Detection Methodology
An episode is classified as a priority inversion if and only if:
1. A high-priority task $T_H$ enters `BLOCKED` state waiting for a resource owned by lower-priority task $T_L$.
2. One or more intermediate-priority tasks $T_M$ ($Priority(T_L) < Priority(T_M) < Priority(T_H)$) execute during $T_H$'s blocked interval, delaying $T_L$'s release of the resource.
3. The framework differentiates direct resource blocking duration ($T_L$'s required critical section execution) from additional interference delay introduced by $T_M$.

