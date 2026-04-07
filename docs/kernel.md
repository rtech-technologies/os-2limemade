# OSx2 Limemade Kernel Internal Architecture

## 1. Physical Memory Model
The kernel executes at the 2MB physical address mark (`0x200000`) for direct hardware mapping and stability.

## 2. Active-Relay Scheduling
Tasks yield control voluntarily using `sys_yield()`, which triggers an `int $0x81`. The scheduler uses a round-robin approach, skipping tasks in the `TASK_WAITING` state.

## 3. Sovereign Slab Isolation
Each task is assigned a 4MB slab for memory allocations, ensuring that a single task cannot consume the entire system heap.

## 4. Hardware Services
Drivers are modular and registered in the `services.c` registry. Events like `EVENT_INIT` and `EVENT_MAIN` drive the system lifecycle.
