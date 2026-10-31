# Builds every example with the flags used on the pages.
#   make            build everything into build/
#   make clean      remove build/
#   make check-sync verify the .c files match the code shown in docs/ (needs python3)
#
# Optimisation levels are deliberate:
#   race      -O0  so lost updates are easy to see (at -O2 they often hide)
#   ryzen_lab -O2  as the page requires (its racy code uses volatile)
#   single_count -O2, as measured on the page
#   everything else: GCC default (-O0), as in the pages' build commands

CC     ?= gcc
CFLAGS  = -std=gnu90 -Wall -Wextra -pedantic -pthread
B       = build

ifeq ($(OS),Windows_NT)
EXE = .exe
endif

PROGS = fork hello_threads race mutex single_count deadlock deadlock_fixed \
        condvar counter_original counter_fixed ryzen_lab
BINS  = $(addprefix $(B)/,$(addsuffix $(EXE),$(PROGS)))

all: $(BINS)

$(B):
	mkdir -p $(B)

$(B)/fork$(EXE):             c/01-processes/fork.c          | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/hello_threads$(EXE):    c/02-threads/hello_threads.c   | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/race$(EXE):             c/03-race/race.c               | $(B) ; $(CC) $(CFLAGS) -O0 $< -o $@
$(B)/mutex$(EXE):            c/03-race/mutex.c              | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/single_count$(EXE):     c/03-race/single_count.c       | $(B) ; $(CC) $(CFLAGS) -O2 $< -o $@
$(B)/deadlock$(EXE):         c/04-deadlock/deadlock.c       | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/deadlock_fixed$(EXE):   c/04-deadlock/deadlock_fixed.c | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/condvar$(EXE):          c/05-condvar/condvar.c         | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/counter_original$(EXE): c/05-condvar/counter_original.c| $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/counter_fixed$(EXE):    c/05-condvar/counter_fixed.c   | $(B) ; $(CC) $(CFLAGS)     $< -o $@
$(B)/ryzen_lab$(EXE):        c/06-hardware/ryzen_lab.c      | $(B) ; $(CC) $(CFLAGS) -O2 $< -o $@

check-sync:
	python3 scripts/check_sync.py

clean:
	rm -rf $(B)

.PHONY: all clean check-sync
