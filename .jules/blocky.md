2024-05-11 - [Module Index Handover Fix]
Learning: Spawning the wrong Limine module index (e.g., index 2 when only 0 and 1 are loaded) causes an immediate kernel exception or invalid task state, blocking the boot-to-installer flow.
Action: Always verify module indices against the `limine.cfg` and use a safe `tasking_spawn_module` wrapper that checks bounds.
2025-05-14 - [Allocator Mismatch Block] Learning: Using slab_alloc but deallocating with free() caused a 16-byte memory corruption due to slab_header_t mismatch. Action: Always use paired allocation/deallocation functions (malloc/free or slab_alloc/slab_free) and verify header compatibility when using utility libraries like ARC.
