/*
 * Functions for handling the system call tables.
 * These functions are only used by architectures that have either 32 or 64 bit syscalls, but not both.
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "trinity.h"	// ARRAY_SIZE, alloc_shared
#include "arch.h"
#include "syscall.h"
#include "params.h"
#include "log.h"
#include "shm.h"
#include "tables.h"

const struct syscalltable *syscalls;

unsigned int max_nr_syscalls;

void activate_syscall(unsigned int calln)
{
	activate_syscall_in_table(calln, &shm->nr_active_syscalls, syscalls, shm->active_syscalls);
}

void deactivate_syscall(unsigned int calln)
{
	deactivate_syscall_in_table(calln, &shm->nr_active_syscalls, syscalls, shm->active_syscalls);
}

void toggle_syscall_n(int calln, bool state, const char *arg, const char *arg_name)
{
	struct syscall *syscall = syscalls[calln].entry;

	if (calln == -1) {
		outputerr("No idea what syscall (%s) is.\n", arg);
		exit(EXIT_FAILURE);
	}

	validate_specific_syscall(syscalls, calln);

	if (state == TRUE) {
		syscall->flags |= ACTIVE;
		activate_syscall(calln);
	} else {
		syscall->flags |= TO_BE_DEACTIVATED;
	}

	output(0, "Marking syscall %s (%d) as to be %sabled.\n",
		arg_name, calln,
		state ? "en" : "dis");
}


void enable_random_syscalls_uniarch(void)
{
	unsigned int call;
	struct syscall *syscall;

retry:
	call = rand() % max_nr_syscalls;
	syscall = syscalls[call].entry;

	if (validate_specific_syscall_silent(syscalls, call) == FALSE)
		goto retry;

	if (no_files == TRUE)
		if (is_syscall_net_related(syscalls, call) == FALSE)
			goto retry;

	/* if we've set this to be disabled, don't enable it! */
	if (syscall->flags & TO_BE_DEACTIVATED)
		goto retry;

	toggle_syscall_n(call, TRUE, syscall->name, syscall->name);
}

void disable_non_net_syscalls_uniarch(void)
{
	struct syscall *syscall;

	unsigned int i;

	for_each_syscall(i) {
		syscall = syscalls[i].entry;

		if (validate_specific_syscall_silent(syscalls, i) == FALSE)
			continue;

		if (syscall->flags & ACTIVE) {
			if (is_syscall_net_related(syscalls, i) == FALSE)
				toggle_syscall_n(i, FALSE, syscall->name, syscall->name);
		}
	}
}

int setup_syscall_group_uniarch(unsigned int group)
{
	unsigned int i;

	for_each_syscall(i) {
		if (syscalls[i].entry->group == group)
			activate_syscall(i);
	}

	if (shm->nr_active_syscalls == 0) {
		outputstd("No syscalls found in group\n");
		return FALSE;
	} else {
		outputstd("Found %d syscalls in group\n", shm->nr_active_syscalls);
	}

	return TRUE;
}

void mark_all_syscalls_active_uniarch(void)
{
	unsigned int i;

	for_each_syscall(i) {
		syscalls[i].entry->flags |= ACTIVE;
		activate_syscall(i);
	}
}

void init_syscalls_uniarch(void)
{
	struct syscall *syscall;

	unsigned int i;

	for_each_syscall(i) {
		syscall = syscalls[i].entry;
		if (syscall->flags & ACTIVE)
			if (syscall->init)
				syscall->init();
	}
}

void deactivate_disabled_syscalls_uniarch(void)
{
	struct syscall *syscall;
	unsigned int i;

	for_each_syscall(i) {
		syscall = syscalls[i].entry;
		if (syscall->flags & TO_BE_DEACTIVATED) {
			syscall->flags &= ~(ACTIVE|TO_BE_DEACTIVATED);
			deactivate_syscall(i);
			output(0, "Marked syscall %s (%d) as deactivated.\n",
				syscall->name, syscall->number);
		}
	}
}

void dump_syscall_tables_uniarch(void)
{
	struct syscall *syscall;
	unsigned int i;

	outputstd("syscalls: %d\n", max_nr_syscalls);

	for_each_syscall(i) {
		syscall = syscalls[i].entry;
		outputstd("entrypoint %d %s : ", syscall->number, syscall->name);
		show_state(syscall->flags & ACTIVE);
		if (syscall->flags & AVOID_SYSCALL)
			outputstd(" AVOID");
		outputstd("\n");
	}
}

void display_enabled_syscalls_uniarch(void)
{
	struct syscall *syscall;
        unsigned int i;

	for_each_syscall(i) {
		syscall = syscalls[i].entry;

		if (syscall->flags & ACTIVE)
			output(0, "syscall %d:%s enabled.\n", i, syscall->name);
	}
}
