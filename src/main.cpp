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
#include <chrono>

std::mutex outgoing_messages_mtx;
size_t all_vms_bytecode_size;
char *all_vms_bytecode;

///////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////

void vm_thread(VM *vm) {
	while (true) {
		if (vm->have_incoming_message) {
			const std::lock_guard<std::mutex> lock(vm->incoming_message_mutex);
			
			while(!vm->incoming_messages.empty()) {
				VM_Message message = vm->incoming_messages.front();
				bool free_data = true;

				//fprintf(stderr, "Received something\n");
				switch (message.type) {
					case VM_MESSAGE_PING:
						vm->send_message(VM_MESSAGE_PONG, message.other_id, message.status, nullptr, 0);
						break;
					case VM_MESSAGE_VERSION_CHECK:
						vm->send_message(VM_MESSAGE_VERSION_CHECK, 0, 1, nullptr, 0);
						break;
					case VM_MESSAGE_SHUTDOWN:
						break;
					case VM_MESSAGE_RUN_CODE:
						vm->run_code_on_script(message.entity_id, (const char*)message.data, message.data_len);
						break;
					case VM_MESSAGE_START_SCRIPT:
						vm->add_script(message.entity_id);
						break;
					case VM_MESSAGE_STOP_SCRIPT:
						vm->remove_script(message.entity_id);
						break;
					case VM_MESSAGE_API_CALL:
						break;
					case VM_MESSAGE_API_CALL_GET:
						fprintf(stderr, "Got response key %d\n", message.other_id);
						vm->api_results[message.other_id] = message;
						free_data = false;
						break;
					case VM_MESSAGE_CALLBACK:
						break;
					default:
						break;
				}

				// Remove it from the queue
				vm->incoming_messages.pop();
				if (free_data && message.data)
					free(message.data);
			}

			// Replace the promise and future
			vm->incoming_message_promise = std::promise<void>();
			vm->incoming_message_future = vm->incoming_message_promise.get_future();
			vm->have_incoming_message = false;
		}

		RunThreadsStatus status = vm->run_scripts();
		fprintf(stderr, "VM run scripts status: %d\n", status);
		switch (status) {
			case RUN_THREADS_ALL_WAITING:
				puts("All threads are waiting");
				if(vm->is_any_script_sleeping) {
					fprintf(stderr, "Sleeping...\n");
					struct timespec now_ts;
					clock_gettime(CLOCK_MONOTONIC, &now_ts);
					if (is_ts_earlier(now_ts, vm->earliest_wake_up_at)) {
						timespec d = diff_ts(now_ts, vm->earliest_wake_up_at);
						vm->incoming_message_future.wait_for(std::chrono::nanoseconds(d.tv_sec * ONE_SECOND_IN_NANOSECONDS + d.tv_nsec));
					} else {
						fprintf(stderr, "Waiting for something in the past");
						continue;
					}
				} else {
					vm->incoming_message_future.wait();
				}
				break;
			case RUN_THREADS_FINISHED:
				vm->incoming_message_future.wait();
				break;
			default:
				break;
		}
	}
}

void test(VM *l) {
	std::thread t1(vm_thread, l);

//	const char *test_script = "print(\"Hello!\")";
//	const char *test_script = "print(tt.from_json(\"[1, 2, 3, 4]\"))";
//	const char *test_script = "local e = entity.me(); print(\"it's\"); print(e); e:say(\"Hello world\"); print(\"Printing too\");";
	const char *test_script = "print(\"going to call function\"); print(storage.load(\"loading test\")); print(\"after the load\")";

	std::this_thread::sleep_for(std::chrono::seconds(2));
	l->receive_message(VM_MESSAGE_START_SCRIPT, 5, 0, 0, nullptr, 0);
	l->receive_message(VM_MESSAGE_RUN_CODE, 5, 0, 0, (void*)test_script, strlen(test_script)+MESSAGE_HEADER_SIZE);
	std::this_thread::sleep_for(std::chrono::seconds(2));
//	std::this_thread::sleep_for(std::chrono::seconds(2));

	char api_got[] = {3, 123, 0, 0, 0};
	l->receive_message(VM_MESSAGE_API_CALL_GET, 5, 1, 1, (void*)api_got, strlen(test_script)+MESSAGE_HEADER_SIZE);

	t1.join();
}

int main(void) {
	// Compile the global script first
	const char *script_to_load_into_all_vms = "for k, v in {{\"entity\", \"new\"},{\"map\", \"who\"},{\"map\", \"turf_at\"},{\"map\", \"objs_at\"},{\"map\", \"dense_at\"},{\"map\", \"tile_info\"},{\"map\", \"map_info\"},{\"map\", \"within_map\"},{\"storage\", \"load\"},{\"storage\", \"list\"},{\"Entity\", \"who\"},{\"Entity\", \"clone\"}} do local original = _G[v[1]][v[2]]; _G[v[1]][v[2]] = function(...) original(unpack({...})); return tt._result(); end; end";

	all_vms_bytecode = luau_compile(script_to_load_into_all_vms, strlen(script_to_load_into_all_vms), NULL, &all_vms_bytecode_size);

	VM l = VM(1);
	test(&l);
}
