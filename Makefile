CC     = gcc
CFLAGS = -Wall -Wextra -g -Isrc

SRC_COMMON = src/allocator.c src/slab.c src/pool.c

all: baseline custom

baseline: src/main_malloc.c
	$(CC) $(CFLAGS) src/main_malloc.c -o baseline

custom: $(SRC_COMMON) src/custom_alloc_sim.c
	$(CC) $(CFLAGS) $(SRC_COMMON) src/custom_alloc_sim.c -o custom

run: all
	@echo "\n--- Running baseline ---"
	./baseline
	@echo "\n--- Running custom allocator ---"
	./custom

clean:
	rm -f baseline custom