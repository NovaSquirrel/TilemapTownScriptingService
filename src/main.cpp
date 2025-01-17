/* 
 * Tilemap Town Scripting Service
 *
 * Copyright (C) 2025 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "scripting.hpp"
#include <unistd.h>
#include <thread>

// From https://stackoverflow.com/a/6749766
timespec diff_ts(timespec start, timespec end) {
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}

bool is_ts_earlier(timespec now, timespec future) {
	return (now.tv_sec < future.tv_sec) || (now.tv_sec == future.tv_sec && now.tv_nsec < future.tv_nsec);
}

void vm_thread(VM *vm) {
	vm->add_script(5, "eee = {}; for i = 1,10 do eee[i] = i; print(i) end");
	vm->add_script(6, "print(1)");
	vm->add_script(7, "print(123)");
	vm->add_script(8, "print(1234)");
	vm->add_script(9, "print(12345)");
	vm->add_script(10, "while true do print(\"eep\"); tt.sleep(1000); end");
//	vm->add_script(11, "while true do end");

	RunThreadsStatus status;
	do {
		status = vm->run_scripts();
		printf("VM run scripts status: %d\n", status);
		if (status == RUN_THREADS_ALL_WAITING) {
			puts("All threads are waiting");
			if(vm->is_any_script_sleeping) {
				puts("Sleeping...");
				struct timespec now_ts;
				clock_gettime(CLOCK_MONOTONIC, &now_ts);
				if (is_ts_earlier(now_ts, vm->earliest_wake_up_at)) {
					timespec d = diff_ts(now_ts, vm->earliest_wake_up_at);
					sleep(d.tv_sec);
					usleep(d.tv_nsec / 1000);
				} else {
					usleep(100000);
				}
			} else {
				usleep(100000);
			}
		}
	} while(status != RUN_THREADS_FINISHED);

	vm->remove_script(5);
	vm->remove_script(6);
	vm->remove_script(7);
	vm->remove_script(8);
	vm->remove_script(9);
	vm->remove_script(10);
//	vm->remove_script(11);
}

void test(VM *l) {
	std::thread t1(vm_thread, l);
	t1.join();
}

int main(void) {
	VM l = VM();
	test(&l);
}
