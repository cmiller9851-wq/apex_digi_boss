/*
 * Architecture: Production Hardened Phase-Shift Ring Matrix (MPMC Lock-Free)
 * Target: x86_64 / POSIX Low-Latency Systems
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <immintrin.h>

#define MATRIX_PAGE_BITS   10
#define MATRIX_PAGE_SLOTS  (1ULL << MATRIX_PAGE_BITS)
#define MATRIX_PAGE_MASK   (MATRIX_PAGE_SLOTS - 1)

#define NUM_PAGE_BITS      3
#define NUM_VIRTUAL_PAGES  (1ULL << NUM_PAGE_BITS)
#define VIRTUAL_PAGE_MASK  (NUM_VIRTUAL_PAGES - 1)

#define TOTAL_CAPACITY     (MATRIX_PAGE_SLOTS * NUM_VIRTUAL_PAGES)

typedef struct __attribute__((aligned(64))) {
    _Atomic uint64_t phase_stamp;
    _Atomic uint64_t payload;
} MatrixSlot;

typedef struct {
    MatrixSlot slots[MATRIX_PAGE_SLOTS];
} MatrixPage;

typedef struct __attribute__((aligned(64))) {
    _Atomic uint64_t head_index;
    _Atomic uint64_t tail_index;
    MatrixPage*      pages[NUM_VIRTUAL_PAGES];
} PhaseShiftMatrix;

PhaseShiftMatrix* ps_matrix_init(void) {
    PhaseShiftMatrix* matrix = (PhaseShiftMatrix*)mmap(
        NULL, sizeof(PhaseShiftMatrix),
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
    );

    if (matrix == MAP_FAILED) return NULL;

    atomic_store_explicit(&matrix->head_index, 0, memory_order_relaxed);
    atomic_store_explicit(&matrix->tail_index, 0, memory_order_relaxed);

    for (size_t i = 0; i < NUM_VIRTUAL_PAGES; ++i) {
        matrix->pages[i] = (MatrixPage*)mmap(
            NULL, sizeof(MatrixPage),
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
        );

        for (size_t s = 0; s < MATRIX_PAGE_SLOTS; ++s) {
            uint64_t global_slot_id = (i << MATRIX_PAGE_BITS) | s;
            atomic_store_explicit(&matrix->pages[i]->slots[s].phase_stamp, global_slot_id, memory_order_relaxed);
            atomic_store_explicit(&matrix->pages[i]->slots[s].payload, 0, memory_order_relaxed);
        }
    }

    return matrix;
}

/**
 * Thread-Safe MPMC Enqueue
 */
bool ps_matrix_enqueue(PhaseShiftMatrix* matrix, uint64_t data) {
    MatrixSlot* slot;
    uint64_t head = atomic_load_explicit(&matrix->head_index, memory_order_relaxed);

    while (1) {
        uint64_t page_idx = (head >> MATRIX_PAGE_BITS) & VIRTUAL_PAGE_MASK;
        uint64_t slot_idx = head & MATRIX_PAGE_MASK;
        
        slot = &matrix->pages[page_idx]->slots[slot_idx];
        uint64_t stamp = atomic_load_explicit(&slot->phase_stamp, memory_order_acquire);

        int64_t diff = (int64_t)stamp - (int64_t)head;

        if (diff == 0) {
            // Claim ticket globally FIRST
            if (atomic_compare_exchange_weak_explicit(
                    &matrix->head_index, &head, head + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break; // Claim succeeded. This thread uniquely owns 'head' write rights to this slot.
            }
        } else if (diff < 0) {
            // Full queue check
            return false;
        } else {
            head = atomic_load_explicit(&matrix->head_index, memory_order_relaxed);
            _mm_pause();
        }
    }

    // Write payload safely—no other producer can claim this slot
    atomic_store_explicit(&slot->payload, data, memory_order_relaxed);
    
    // Release barrier guarantees payload write is visible BEFORE phase_stamp update
    atomic_store_explicit(&slot->phase_stamp, head + 1, memory_order_release);
    return true;
}

/**
 * Thread-Safe MPMC Dequeue
 */
bool ps_matrix_dequeue(PhaseShiftMatrix* matrix, uint64_t* out_data) {
    MatrixSlot* slot;
    uint64_t tail = atomic_load_explicit(&matrix->tail_index, memory_order_relaxed);

    while (1) {
        uint64_t page_idx = (tail >> MATRIX_PAGE_BITS) & VIRTUAL_PAGE_MASK;
        uint64_t slot_idx = tail & MATRIX_PAGE_MASK;

        slot = &matrix->pages[page_idx]->slots[slot_idx];
        uint64_t stamp = atomic_load_explicit(&slot->phase_stamp, memory_order_acquire);

        int64_t diff = (int64_t)stamp - (int64_t)(tail + 1);

        if (diff == 0) {
            // Claim ticket globally FIRST
            if (atomic_compare_exchange_weak_explicit(
                    &matrix->tail_index, &tail, tail + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break; // Claim succeeded. This thread uniquely owns 'tail' read rights.
            }
        } else if (diff < 0) {
            // Empty queue check
            return false;
        } else {
            tail = atomic_load_explicit(&matrix->tail_index, memory_order_relaxed);
            _mm_pause();
        }
    }

    // Read payload safely
    *out_data = atomic_load_explicit(&slot->payload, memory_order_relaxed);

    // Release slot back to producers for the next epoch loop
    atomic_store_explicit(&slot->phase_stamp, tail + TOTAL_CAPACITY, memory_order_release);
    return true;
}

int main(void) {
    printf("[*] Running Corrected Low-Latency MPMC Queue Engine...\n");

    PhaseShiftMatrix* matrix = ps_matrix_init();
    if (!matrix) return 1;

    // Single thread verification
    for (uint64_t i = 1; i <= 5; ++i) {
        ps_matrix_enqueue(matrix, i * 100);
    }

    uint64_t val = 0;
    while (ps_matrix_dequeue(matrix, &val)) {
        printf("  [<-] Popped Data: %lu\n", val);
    }

    return 0;
}
