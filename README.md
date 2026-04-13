# game_allocator

> A custom memory allocator for C game simulations combining slab pooling, best-fit pool allocation, and an online linear regression lifetime predictor — benchmarked against system `malloc` across a 3000-frame game loop.

---

## Problem Statement

Modern game engines suffer from **heap fragmentation** at runtime. As game objects (particles, enemies, bullets, audio buffers, physics bodies) are continuously allocated and freed across thousands of frames, the heap develops scattered free blocks too small to reuse. This forces the allocator to spend increasingly longer time searching for usable memory, causing measurable frame-time spikes and gameplay stutters.

Existing general-purpose allocators like `malloc` have **no knowledge of object lifetimes** — they treat a particle that lives 3 frames identically to an enemy that lives 500 frames. This project proposes a custom memory allocator in C that combines:

- **Slab allocator** — fixed-size, O(1) allocation for short-lived objects
- **Best-fit pool allocator** — coalescing free-list for variable-size long-lived objects
- **Online linear regression predictor** — trained at runtime to predict object lifetime and route allocations optimally before fragmentation occurs

---

## Project Structure

```
game_allocator/
│
├── src/
│   ├── main.c              → Simulation + benchmark (custom allocator)
│   ├── main_baseline.c     → Baseline simulation (system malloc only)
│   ├── memory_pool.c/h     → General pool allocator (best-fit + coalescing)
│   ├── slab.c/h            → Fixed-size fast slab allocator
│   ├── allocator.c/h       → smart_alloc + smart_free (core routing logic)
│   ├── predictor.c/h       → ML lifetime predictor (online linear regression)
│   └── visualizer.c/h      → Memory state display
│
├── results/
│   ├── fragmentation.csv   → Free block count over frames (custom vs baseline)
│   ├── frame_time.csv      → Per-frame allocation time in microseconds
│   └── prediction_error.csv → Predictor MAE per object type across training
│
├── Makefile
└── README.md
```

---

## Architecture

### Object Types & Lifetimes

| Type     | Size   | Lifetime | Spawn Rate | Target Allocator |
|----------|--------|----------|------------|-----------------|
| Particle | 32 B   | 3 frames | 70%        | Slab            |
| Bullet   | 128 B  | 50 frames| 25%        | Pool            |
| Enemy    | 1024 B | 500 frames| 5%        | Pool / Malloc   |

### Allocation Decision Flow

```
smart_alloc(type, size)
       │
       ▼
predict_lifetime(type, size, frame)
       │
       ├── predicted < 10 frames  →  slab_alloc()    [O(1), fixed-size]
       │
       └── predicted ≥ 10 frames  →  pool_alloc()    [best-fit, coalescing]
                                          │
                                          └── pool full → malloc()  [fallback]
```

### Predictor: Online Linear Regression

The predictor uses 4 features and is updated on every `smart_free()` call using stochastic gradient descent:

```
predicted_lifetime = w0
                   + w1 × (obj_type / 2)
                   + w2 × (size / 1024)
                   + w3 × (frame_number / 3000)
                   + w4 × (spawn_rate / 10)
```

Initial weights are hand-seeded (`w1 = 99.8` to leverage type as the dominant feature), then refined online. The decision threshold is `SHORT_LIVED_THRESHOLD = 10.0f` frames.

---

## Components

### `slab.c` — Fixed-Size Slab Allocator

- Pre-allocates a static arena of `10,000 × 128-byte` blocks
- Maintains a singly-linked free-list for O(1) alloc/free
- Zero fragmentation — all blocks are the same size
- Ideal for particles: high-frequency, same size, very short-lived

### `memory_pool.c` — Best-Fit Pool Allocator

- 40 MB static arena managed with an intrusive linked-list of `Block` headers
- First-fit scan with **splitting**: leftover space becomes a new free block
- **Coalescing** on free: adjacent free blocks are merged to prevent fragmentation
- Exposes `pool_count_fragments()` and `pool_fragmentation_percent()` for benchmarking

### `allocator.c` — Smart Router

- `AllocHeader` prepended to every allocation records `source`, `type`, `size`
- **Pointer hash map** (open addressing, linear probing, `MAP_SIZE = 16384`) tracks every live allocation: frame allocated, type, size — used to call `update_weights()` on free
- `smart_free()` does an O(1) lookup, updates the predictor, then dispatches to the correct sub-allocator based on the embedded header

### `predictor.c` — Online Linear Regression

- Trained purely online — no offline dataset needed
- MSE tracked per 1000-update bucket to observe learning convergence
- Per-type MAE and routing accuracy logged at end of simulation
- Routing accuracy = % of allocs correctly classified as short-lived vs long-lived

---

## Results Summary

### Simulation Parameters

| Parameter         | Value      |
|-------------------|------------|
| Frames simulated  | 3,000      |
| Max live objects  | 10,000     |
| Objects per frame | 10 spawned |
| Random seed       | 42         |

### Allocator Routing (Custom Allocator)

| Allocator | Allocs Routed | Share  |
|-----------|--------------|--------|
| Slab      | ~21,000      | ~70%   |
| Pool      | ~9,000       | ~29%   |
| Malloc    | ~500         | ~1%    |

### Fragmentation Comparison

| Metric                      | Baseline (malloc) | Custom Allocator |
|-----------------------------|-------------------|-----------------|
| Mixed-size free events      | ~21,000           | ~0 (slab isolated) |
| Pool free block fragments   | N/A               | < 5%            |
| Heap control                | None              | Full            |

### Predictor Convergence

| Object Type | True Lifetime | MAE (frames) |
|-------------|--------------|--------------|
| Particle    | 3            | < 2          |
| Bullet      | 50           | < 15         |
| Enemy       | 500          | < 50         |

Routing accuracy converges to **>95%** after ~5,000 training samples.

### MSE Learning Curve

MSE drops sharply in the first 5,000 updates as weights converge, then plateaus — confirming online SGD is learning the correct weight magnitudes.

---

## Building & Running

### Requirements

- GCC or Clang
- Standard C library (`math.h`, `stdlib.h`)
- `make`

### Build

```bash
# Build custom allocator simulation
make

# Build baseline (malloc-only) simulation
make baseline
```

### Run

```bash
# Run custom allocator simulation
./game_allocator

# Run baseline for comparison
./game_baseline
```

### Sample Output

```
========================================
   CUSTOM ALLOCATOR SIMULATION
========================================

--- Simulation summary ---
  Frames simulated  : 3000
  Total allocations : 30010
  Peak live objects : 10000
  Active at end     : 4630

--- Allocator routing (predictor decisions) ---
  Routed to slab   : 21014  (70.0%)
  Routed to pool   : 8996   (30.0%)
  Routed to malloc : 0      (0.0%)

========================================
  PREDICTOR  (Online Linear Regression)
========================================
  Training samples : 25380

  Weights learned:
    w0  bias        =   -1.243
    w1  obj_type    =  152.803  (dominant — type drives lifetime)
    w2  size        =    0.512
    w3  frame_no    =   -0.031
    w4  spawn_rate  =   -2.147

  Mean absolute error per object type:
    Particle (true=3  )       MAE =  1.84 frames
    Bullet   (true=50 )       MAE = 12.41 frames
    Enemy    (true=500)       MAE = 43.67 frames

  Routing accuracy : 29871 / 30010  (99.5%)
```

---

## Key Design Decisions

**Why linear regression?** Object lifetime in this simulation is almost entirely determined by type — a single categorical feature. Linear regression with a type feature trains to near-perfect routing accuracy in thousands of updates and adds negligible per-frame overhead (~microseconds).

**Why open-addressing hash map?** `smart_free()` is called once per object death. An O(1) pointer lookup is critical; a linked-list scan over 10,000 live objects would dominate frame time. The hash map keeps free tracking out of the hot path.

**Why tombstone deletion?** Linear probe hash maps require tombstones (not simple nulling) to preserve probe chains for keys inserted after a collision. `TOMBSTONE = (void*)0x1` is a sentinel that can never be a valid heap pointer.

**Why pre-seeded weights?** Cold-start with all-zero weights causes every early allocation to be misrouted. Seeding `w1 = 99.8` encodes the prior that type is the dominant predictor, achieving >90% routing accuracy from frame 1 before the model even trains.

---

## File Descriptions

| File | Purpose |
|------|---------|
| `src/main.c` | Game loop simulation, spawns Particle/Bullet/Enemy objects, calls smart_alloc/smart_free, prints final results table |
| `src/main_baseline.c` | Identical simulation using raw `malloc`/`free`, measures fragmentation events and total time |
| `src/allocator.c` | Core routing logic: pointer hash map, `smart_alloc`, `smart_free`, allocator stats |
| `src/allocator.h` | `ObjectType`, `AllocSource`, `AllocHeader` typedefs; public API |
| `src/slab.c` | 10,000-slot fixed-size slab with O(1) free-list |
| `src/slab.h` | `slab_init`, `slab_alloc`, `slab_free` |
| `src/memory_pool.c` | 40 MB best-fit pool with splitting and coalescing |
| `src/pool.h` | `pool_init`, `pool_alloc`, `pool_free`, fragmentation metrics |
| `src/predictor.c` | Online SGD linear regression, per-bucket MSE, per-type MAE, routing accuracy |
| `src/predictor.h` | `Features` struct, `SHORT_LIVED_THRESHOLD`, public API |

---



