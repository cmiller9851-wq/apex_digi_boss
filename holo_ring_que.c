// 1. Claim global position
FIRST via CAS
if (atomic_compare_exchange _ weak_explicit&matrix-
>head_index, &head, head +
...
/I 2. Write payload SECOND
atomic_store_explicit&slot-
>payload, data,
memory_order_relaxed);
// 3. Update stamp THIRD
atomic_store_explicit&slot-
›phase_stamp, head + 1, memory_order_release);
return true;
｝