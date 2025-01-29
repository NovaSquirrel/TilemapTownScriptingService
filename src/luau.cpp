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
#include <stdexcept>

//#define SCHEDULING_PRINTS 1

///////////////////////////////////////////////////////////

extern size_t all_vms_bytecode_size;
extern char *all_vms_bytecode;

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
void put_32(unsigned int x) {
	putchar((x) & 255);
	putchar((x >> 8) & 255);
	putchar((x >> 16) & 255);
	putchar((x >> 24) & 255);
}

void send_outgoing_message(VM_MessageType type, unsigned int user_id, int entity_id, unsigned int other_id, unsigned char status, const void *data, size_t data_len) {
	size_t written_len = data_len;

	//fprintf(stderr, "Writing an outgoing message\n");

	const std::lock_guard<std::mutex> lock(outgoing_messages_mtx);
	// TT LL-LL-LL UU-UU-UU-UU EE-EE-EE-EE OO-OO-OO-OO SS [rest of it is the arbitrary data]
	putchar(type);
	putchar((written_len)     & 255);
	putchar((written_len>>8)  & 255);
	putchar((written_len>>16) & 255);
	put_32(user_id);
	put_32(entity_id);
	put_32(other_id);
	putchar(status);
	if (data)
		fwrite(data, 1, data_len, stdout);
	fflush(stdout);
}

///////////////////////////////////////////////////////////

void *lua_allocator(void *ud, void *ptr, size_t osize, size_t nsize) {
	class VM *l = (VM*)ud;
	if ((l->total_allocated_memory - osize + nsize) > l->memory_allocation_limit)
		return NULL;

	l->total_allocated_memory -= osize;
	l->total_allocated_memory += nsize;
	//printf("Total allocation: %ld\n", l->total_allocated_memory);

	if (nsize == 0) {
		free(ptr);
		return NULL;
	} else {
		return realloc(ptr, nsize);
	}
}

void callback_userthread(lua_State* LP, lua_State* L) {
	if (LP != NULL) {
		lua_setthreaddata(L, lua_getthreaddata(LP));
	}
}

void callback_allocate(lua_State* L, size_t osize, size_t nsize) {
}
void callback_break(lua_State* L, lua_Debug* ar) {
}

void callback_debuginterrupt(lua_State* L, lua_Debug* ar) {
	ScriptThread *thread = (ScriptThread*)lua_getthreaddata(L);
	if (thread) {
		thread->interrupted = static_cast<lua_State*>(ar->userdata);
	}
}

void callback_interrupt(lua_State* L, int gc) {
	if (gc == -1) {
		ScriptThread *thread = (ScriptThread*)lua_getthreaddata(L);
		if (!thread)
			return;

		// Has the thread been running for too long?
		struct timespec now_ts;
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now_ts);
		if (now_ts.tv_sec < thread->preempt_at.tv_sec) {
			return;
		}
		if (now_ts.tv_sec > thread->preempt_at.tv_sec || (now_ts.tv_sec == thread->preempt_at.tv_sec && now_ts.tv_nsec >= thread->preempt_at.tv_nsec)) {
//			throw std::runtime_error("Script ran for too long");
			thread->script->count_preempts++;
			thread->script->vm->count_preempts++;
			thread->was_preempted = true;
			lua_break(L);
			return;
		}
	}
}

VM::VM(int user_id) {
	fprintf(stderr, "new VM\n");
	this->user_id = user_id;
	this->total_allocated_memory = 0;
	this->memory_allocation_limit = 2*1024*1024;
	this->incoming_message_future = incoming_message_promise.get_future();
	this->have_incoming_message = false;
	this->next_api_result_key = 1;

	this->count_force_terminate = 0;
	this->count_preempts = 0;

	this->L = lua_newstate(lua_allocator, this);
    luaL_openlibs(this->L);
	register_lua_api(this->L);
	this->run_code_on_self(all_vms_bytecode, all_vms_bytecode_size);
	luaL_sandbox(this->L);
	lua_setthreaddata(this->L, NULL);
	lua_gc(this->L, LUA_GCCOLLECT, 0);

	lua_Callbacks* cb = lua_callbacks(this->L);
	cb->userthread = callback_userthread;
//	cb->onallocate = callback_allocate;
	cb->interrupt = callback_interrupt;
//	cb->debugbreak = callback_break;
	cb->debuginterrupt = callback_debuginterrupt;
}

VM::~VM() {
	fprintf(stderr, "del VM\n");
	lua_close(this->L);
}

void VM::add_script(int entity_id) {
	Script *script = new Script(this, entity_id);
	this->scripts[entity_id] = std::unique_ptr<Script>(script);
}

void VM::run_code_on_script(int entity_id, const char *code, size_t code_size) {
	auto it = this->scripts.find(entity_id);
	if(it != this->scripts.end()) {
		Script *script = (*it).second.get();
		script->compile_and_start(code, code_size);
	}
}

void VM::remove_script(int entity_id) {
	auto it = this->scripts.find(entity_id);
	if(it != this->scripts.end()) {
		(*it).second.get()->shutdown();
		this->scripts.erase(it);
	}
}

void VM::run_code_on_self(const char *bytecode, size_t bytecode_size) {
	int result = luau_load(this->L, "init", bytecode, bytecode_size, 0);
	if (result) {
		fprintf(stderr, "Failed to load script into VM: %s\n", lua_tostring(this->L, -1));
		exit(1);
	}
	lua_pcall(L, 0, LUA_MULTRET, 0);
}

RunThreadsStatus VM::run_scripts() {
	#ifdef SCHEDULING_PRINTS
	fprintf(stderr, "VM - running scripts - %ld\n", this->scripts.size());
	#endif
	bool any_scripts_not_done; // At least one script needs to be resumed in the future
	bool all_scripts_waiting;  // All scripts are waiting on a timer or API result or something else
	bool any_preempted_script_skipped_over = false;

	this->is_any_script_sleeping = false;

	bool trying_again_after_clearing_scheduled_flag = false;

	if(this->scripts.empty())
		return RUN_THREADS_FINISHED;
	while (true) {
		any_scripts_not_done = false;
		all_scripts_waiting = true;
		bool any_scripts_newly_scheduled = false;

		for(auto itr = this->scripts.begin(); itr != this->scripts.end(); ++itr) {
			Script *script = (*itr).second.get();
			if (script->was_scheduled_yet) {
				if (script->was_preempted)
					any_preempted_script_skipped_over = true;
				#ifdef SCHEDULING_PRINTS
				fprintf(stderr, "script %p already scheduled\n", script);
				#endif
				continue;
			}
			#ifdef SCHEDULING_PRINTS
			fprintf(stderr, "script %p NOT already scheduled\n", script);
			#endif
			script->was_scheduled_yet = true;
			any_scripts_newly_scheduled = true;

			script->was_preempted = false;
			RunThreadsStatus status = script->run_threads();
			#ifdef SCHEDULING_PRINTS
			fprintf(stderr, "Script status %d\n", status);
			#endif

			switch (status) {
				case RUN_THREADS_FINISHED:
					break;
				case RUN_THREADS_KEEP_GOING:
					all_scripts_waiting = false;
				case RUN_THREADS_ALL_WAITING:
					if (script->is_any_thread_sleeping) {
						if (this->is_any_script_sleeping) {
							if (is_ts_earlier(script->earliest_wake_up_at, this->earliest_wake_up_at)) {
								this->earliest_wake_up_at = script->earliest_wake_up_at;
							}
						} else {
							this->is_any_script_sleeping = true;
							this->earliest_wake_up_at = script->earliest_wake_up_at;
						}
					}
					any_scripts_not_done = true;
					break;
				case RUN_THREADS_PREEMPTED:
					script->was_preempted = true;
					return RUN_THREADS_PREEMPTED;
			}
		}
		
		if (trying_again_after_clearing_scheduled_flag)
			break;
		if (!any_scripts_newly_scheduled) { // Give everything another chance to run, because the entire list was marked as already scheduled
			trying_again_after_clearing_scheduled_flag = true;
			for(auto itr = this->scripts.begin(); itr != this->scripts.end(); ++itr) {
				(*itr).second.get()->was_scheduled_yet = false;
			}
		} else
			break;
	}

	if (!any_scripts_not_done)
		return any_preempted_script_skipped_over ? RUN_THREADS_KEEP_GOING : RUN_THREADS_FINISHED;
	if (all_scripts_waiting)
		return RUN_THREADS_ALL_WAITING;
	return RUN_THREADS_KEEP_GOING;
}

void VM::receive_message(VM_MessageType type, int entity_id, int other_id, unsigned char status, void *data, size_t data_len) {
	VM_Message new_message;
	new_message.type      = type;
	new_message.user_id   = this->user_id;
	new_message.entity_id = entity_id;
	new_message.other_id  = other_id;
	new_message.status    = status;
	new_message.data_len  = data_len;

	if (data != nullptr) {
		new_message.data = malloc(data_len);
		memcpy(new_message.data, data, data_len);
	} else {
		new_message.data  = nullptr;
	}
	time(&new_message.received_at);

	const std::lock_guard<std::mutex> lock(this->incoming_message_mutex);
	this->incoming_messages.push(new_message);
	if (!this->have_incoming_message) {
		this->incoming_message_promise.set_value();
		this->have_incoming_message = true;
	}
}

void VM::send_message(VM_MessageType type, int other_id, unsigned char status, const void *data, size_t data_len) {
	send_outgoing_message(type, this->user_id, 0, other_id, status, data, data_len);
}

void VM::thread_function() {
	bool quitting = false;
	while (!quitting) {
		if (this->have_incoming_message) {
			const std::lock_guard<std::mutex> lock(this->incoming_message_mutex);
			
			while(!this->incoming_messages.empty()) {
				VM_Message message = this->incoming_messages.front();
				bool free_data = true;

				//fprintf(stderr, "Received something\n");
				switch (message.type) {
					case VM_MESSAGE_PING:
						this->send_message(VM_MESSAGE_PONG, message.other_id, message.status, nullptr, 0);
						break;
					case VM_MESSAGE_VERSION_CHECK:
						this->send_message(VM_MESSAGE_VERSION_CHECK, 0, 1, nullptr, 0);
						break;
					case VM_MESSAGE_SHUTDOWN:
						for (auto itr = this->scripts.begin(); itr != this->scripts.end(); ) {
							Script *script = (*itr).second.get();
							script->shutdown();
							itr = this->scripts.erase(itr);
						}
						quitting = true;
						break;
					case VM_MESSAGE_RUN_CODE:
						this->run_code_on_script(message.entity_id, (const char*)message.data, message.data_len);
						break;
					case VM_MESSAGE_START_SCRIPT:
						this->add_script(message.entity_id);
						break;
					case VM_MESSAGE_STOP_SCRIPT:
						this->remove_script(message.entity_id);
						break;
					case VM_MESSAGE_API_CALL:
						break;
					case VM_MESSAGE_API_CALL_GET:
						fprintf(stderr, "Got response key %d\n", message.other_id);
						this->api_results[message.other_id] = message;
						free_data = false;
						break;
					case VM_MESSAGE_CALLBACK:
					{
						//fprintf(stderr, "Got callback type %d\n", message.other_id);

						auto it = this->scripts.find(message.entity_id);
						if(it != this->scripts.end()) {
							(*it).second.get()->start_callback(message.other_id, message.status, message.data, message.data_len);
							free_data = false; // Will handle freeing data above
						} else {
							fprintf(stderr, "Did not find the script");
						}
						break;
					}
					case VM_MESSAGE_STATUS_QUERY:
					{
						char buffer[500];
						sprintf(buffer, "User %d [%ld memory, %ld scripts, %d terminates, %d preempts][ul]", this->user_id, this->total_allocated_memory / 1024, this->scripts.size(), this->count_force_terminate, this->count_preempts);
						std::string str = buffer;

						for(auto itr = scripts.begin(); itr != scripts.end(); ++itr) {
							Script *script = (*itr).second.get();
							sprintf(buffer, "[li]%d<%ld threads, %d terminates, %d preempts>[/li]", script->entity_id, script->threads.size(), script->count_force_terminate, script->count_preempts);
							str += buffer;
						}

						str += "[/ul]";
						const char *c_str = str.c_str();
						this->send_message(VM_MESSAGE_STATUS_QUERY, message.other_id, message.status, c_str, strlen(c_str));
						break;
					}
					default:
						break;
				}

				// Remove it from the queue
				this->incoming_messages.pop();
				if (free_data && message.data)
					free(message.data);
			}

			// Replace the promise and future
			this->incoming_message_promise = std::promise<void>();
			this->incoming_message_future = this->incoming_message_promise.get_future();
			this->have_incoming_message = false;
		}
		if (quitting)
			break;

		RunThreadsStatus status = this->run_scripts();

		// Stop any scripts that have had threads get terminated too many times
		for(auto itr = this->scripts.begin(); itr != this->scripts.end(); ) {
			if ((*itr).second.get()->count_force_terminate >= TERMINATE_SCRIPT_AFTER_STRIKES) {
				//fprintf(stderr, "Stopping script terminated too many times\n");
				itr = this->scripts.erase(itr);
			} else {
				++itr;
			}
		}

		//fprintf(stderr, "VM run scripts status: %d\n", status);
		switch (status) {
			case RUN_THREADS_ALL_WAITING:
				fprintf(stderr, "All threads are waiting\n");
				if (this->is_any_script_sleeping) {
					fprintf(stderr, "Sleeping...\n");
					struct timespec now_ts;
					clock_gettime(CLOCK_MONOTONIC, &now_ts);
					if (is_ts_earlier(now_ts, this->earliest_wake_up_at)) {
						timespec d = diff_ts(now_ts, this->earliest_wake_up_at);
						this->incoming_message_future.wait_for(std::chrono::nanoseconds(d.tv_sec * ONE_SECOND_IN_NANOSECONDS + d.tv_nsec));
					} else {
						fprintf(stderr, "Waiting for something in the past");
						continue;
					}
				} else {
					this->incoming_message_future.wait();
				}
				break;
			case RUN_THREADS_FINISHED:
				this->incoming_message_future.wait();
				break;
			default:
				break;
		}
	}
}

void VM::start_thread() {
	this->thread = std::thread(&VM::thread_function, this);
}

void VM::stop_thread() {
	this->receive_message(VM_MESSAGE_SHUTDOWN, 0, 0, 0, nullptr, 0);
	this->thread.join();
}

///////////////////////////////////////////////////////////

Script::Script(VM *vm, int entity_id) {
	this->vm = vm;
	this->entity_id = entity_id;
	this->was_scheduled_yet = false;

	this->count_force_terminate = 0;
	this->count_preempts = 0;

	// No callbacks
	for (int i=0; i<CALLBACK_COUNT; i++)
		this->callback_ref[i] = LUA_NOREF;

	// Create a thread and store a reference away to prevent it from getting garbage collected
	this->L = lua_newthread(vm->L);
	this->thread_reference = lua_ref(vm->L, -1);
	lua_pop(vm->L, 1);
	lua_setthreaddata(this->L, NULL);

	luaL_sandboxthread(this->L);
}

Script::~Script() {
	// Remove the reference to allow the thread to get garbage collected
	lua_unref(this->vm->L, this->thread_reference);

	lua_resetthread(this->L);
	lua_gc(this->L, LUA_GCCOLLECT, 0);
}

bool Script::compile_and_start(const char *source, size_t source_len) {
	if (this->threads.size() >= MAX_SCRIPT_THREAD_COUNT) {
		//fprintf(stderr, "Too many script threads! Entity %d\n", this->entity_id);
		return true;
	}

	size_t bytecodeSize = 0;

	char* bytecode = luau_compile(source, source_len, NULL, &bytecodeSize);
	if (!bytecode) {
		fprintf(stderr, "No bytecode returned from compile\n");
		return true;
	}
	//fprintf(stderr, "Compiled; %ld bytes\n", bytecodeSize);

	char chunk_name[32];
	if (this->entity_id >= 0) {
		sprintf(chunk_name, "=[entity %d]", this->entity_id);
	} else {
		sprintf(chunk_name, "=[entity ~%d]", -this->entity_id);
	}
	int result = luau_load(this->L, chunk_name, bytecode, bytecodeSize, 0);
	if (result) {
		//fprintf(stderr, "Failed to load script: %s\n", lua_tostring(this->L, -1));
		const char *error = lua_tostring(this->L, -1);
		send_outgoing_message(VM_MESSAGE_SCRIPT_ERROR, this->vm->user_id, this->entity_id, 0, 1, error, strlen(error));
		return true;
	}

	// Try running the thread here, but if it doesn't finish then add it to a list to run later
	ScriptThread *thread = new ScriptThread(this);
	lua_xmove(this->L, thread->L, 1);
	if(thread->run(0)) {
		delete thread;
		return true;
	} else {
		this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
		return false;
	}
}

bool Script::start_callback(int callback_id, int data_item_count, void *data, size_t data_len) {
	if (this->threads.size() >= MAX_SCRIPT_THREAD_COUNT) {
		//fprintf(stderr, "Too many script threads! Entity %d\n", this->entity_id);
		return true;
	}
	if (callback_id < 0 || callback_id >= CALLBACK_COUNT || this->callback_ref[callback_id] == LUA_NOREF) {
		if (data)
			free(data);
		return true; // Technically it's finished, because it never even had to start
	}

	ScriptThread *thread = new ScriptThread(this);
	lua_getref(thread->L, this->callback_ref[callback_id]);
	int arg_count = push_values_from_message_data(thread->L, data_item_count, (char*)data, data_len);
	if(thread->run(arg_count)) {
		delete thread;
		return true;
	} else {
		this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
		return false;
	}
}

bool Script::start_thread(lua_State *from) {
	if (this->threads.size() >= MAX_SCRIPT_THREAD_COUNT) {
		//fprintf(stderr, "Too many script threads! Entity %d\n", this->entity_id);
		return true;
	}

	ScriptThread *thread = new ScriptThread(this);
	lua_xmove(from, thread->L, 1);
	this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
	return false;
}

void update_thread_wakeup_time(Script *s, ScriptThread *thread) {
	if (s->is_any_thread_sleeping) {
		if (is_ts_earlier(thread->wake_up_at, s->earliest_wake_up_at)) {
			s->earliest_wake_up_at = thread->wake_up_at;
		}
	} else {
		s->is_any_thread_sleeping = true;
		s->earliest_wake_up_at = thread->wake_up_at;
	}
}

RunThreadsStatus Script::run_threads() {
	#ifdef SCHEDULING_PRINTS
	fprintf(stderr, "\tScript %p - running scripts - %ld\n", this, this->threads.size());
	#endif
	bool any_threads_run = false;
	bool trying_again_after_clearing_scheduled_flag = false;
	bool any_preempted_thread_skipped_over = false;
	this->is_any_thread_sleeping = false;

	if(this->threads.empty())
		return RUN_THREADS_FINISHED;
	while (true) {
		bool any_threads_newly_scheduled = false;

		for (auto itr = this->threads.begin(); itr != this->threads.end(); ) {
			ScriptThread *thread = (*itr).get();
			if (thread->was_scheduled_yet) {
				if (thread->was_preempted)
					any_preempted_thread_skipped_over = true;
				#ifdef SCHEDULING_PRINTS
				fprintf(stderr, "\tthread %p already scheduled\n", thread);
				#endif
				++itr;
				continue;
			}
			#ifdef SCHEDULING_PRINTS
			fprintf(stderr, "\tthread %p NOT already scheduled\n", thread);
			#endif
			thread->was_scheduled_yet = true;
			any_threads_newly_scheduled = true;

			// If thread is sleeping, check if it's time to wake up
			if (thread->is_sleeping) {
				struct timespec now_ts;
				clock_gettime(CLOCK_MONOTONIC, &now_ts);
				if (now_ts.tv_sec < thread->wake_up_at.tv_sec) {
					update_thread_wakeup_time(this, thread);
					++itr;
					continue;
				}
				if (now_ts.tv_sec > thread->wake_up_at.tv_sec || (now_ts.tv_sec == thread->wake_up_at.tv_sec && now_ts.tv_nsec >= thread->wake_up_at.tv_nsec)) {
					thread->is_sleeping = false;
				} else {
					update_thread_wakeup_time(this, thread);
					++itr;
					continue;
				}
			}

			// If thread is waiting for an API response, check if it's ready yet
			if (thread->is_waiting_for_api) {
				auto it = this->vm->api_results.find(thread->api_response_key);
				if(it != this->vm->api_results.end()) {
					//fprintf(stderr, "Unblocking thread because it got %d\n", thread->api_response_key);

					// Value will be retrieved via tt._result()
					thread->is_waiting_for_api = false;
				} else {
					// Still waiting for API response; waiting too long?
					time_t now = time(NULL);
					if (now - thread->started_waiting_for_api_at >= API_RESULT_TIMEOUT_IN_SECONDS) {
						thread->is_waiting_for_api = false;
					} else {
						++itr;
						continue;
					}
				}
			}

			// Thread is not sleeping, or has just woken up
			bool old_any_threads_run = false;
			any_threads_run = true;
			if (thread->run(0)) {
				itr = this->threads.erase(itr);
			} else {
				if (thread->was_preempted) { // If it was preempted, stop running any other threads
					#ifdef SCHEDULING_PRINTS
					fprintf(stderr, "\tthread was preempted\n");
					#endif
					return RUN_THREADS_PREEMPTED;
				}
				if (thread->is_sleeping && !old_any_threads_run) { // If this thread just fell asleep, allow it to contribute to detecting all threads sleeping
					update_thread_wakeup_time(this, thread);
					any_threads_run = false;
				}
				++itr;
			}
		}

		if (trying_again_after_clearing_scheduled_flag)
			break;
		if(!any_threads_newly_scheduled) { // Give everything another chance to run, because the entire list was marked as already scheduled
			trying_again_after_clearing_scheduled_flag = true;
			for(auto itr = this->threads.begin(); itr != this->threads.end(); ++itr) {
				(*itr).get()->was_scheduled_yet = false;
			}
		} else
			break;
	}

	if(this->threads.empty())
		return any_preempted_thread_skipped_over ? RUN_THREADS_KEEP_GOING : RUN_THREADS_FINISHED;
	if(!any_threads_run)
		return RUN_THREADS_ALL_WAITING;
	return RUN_THREADS_KEEP_GOING;
}

bool Script::shutdown() {
	fprintf(stderr, "Shutting down script\n");
	this->start_callback(CALLBACK_MISC_SHUTDOWN, 0, nullptr, 0);
	return true;
}

///////////////////////////////////////////////////////////

ScriptThread::ScriptThread(Script *s) {
	this->script = s;

	// Create a thread and store a reference away to prevent it from getting garbage collected
	this->L = lua_newthread(s->L);
	this->thread_reference = lua_ref(s->L, -1);
	lua_setthreaddata(this->L, this);
	lua_pop(s->L, 1);

	// Initialize members
	this->interrupted = nullptr;
	this->nanoseconds = 0;
	this->count_force_sleeps = 0;
	this->is_sleeping = false;
	this->is_waiting_for_api = false;
	this->is_thread_stopped = false;
	this->was_scheduled_yet = false;
}

ScriptThread::~ScriptThread() {
	// Remove the reference to allow the thread to get garbage collected
	this->stop();
}

int ScriptThread::resume_script_thread_with_stopwatch(lua_State *state, int arg_count) {
	this->was_preempted = false;

	struct timespec start_ts, end_ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_ts);
	unsigned long long start_nanoseconds = start_ts.tv_sec * ONE_SECOND_IN_NANOSECONDS + start_ts.tv_nsec;
	unsigned long long halt_nanoseconds = start_nanoseconds + TIME_SLICE_IN_NANOSECONDS;
	this->preempt_at.tv_sec  = halt_nanoseconds / ONE_SECOND_IN_NANOSECONDS;
	this->preempt_at.tv_nsec = halt_nanoseconds % ONE_SECOND_IN_NANOSECONDS;

	int status = lua_resume(state, NULL, arg_count);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_ts);

	unsigned long long end_nanoseconds = end_ts.tv_sec * ONE_SECOND_IN_NANOSECONDS + end_ts.tv_nsec;
	unsigned long long nanoseconds = end_nanoseconds - start_nanoseconds;
	this->nanoseconds += nanoseconds;

	if(this->nanoseconds > PENALTY_THRESHOLD_MS * ONE_MILLISECOND_IN_NANOSECONDS) {
		//fprintf(stderr, "Making a thread sleep\n");
		this->count_force_sleeps++;
		if (this->count_force_sleeps >= TERMINATE_THREAD_AFTER_STRIKES) {
			//fprintf(stderr, "Stopping a stuck thread\n");
			this->stop();
			this->script->count_force_terminate++;
			this->script->vm->count_force_terminate++;
			return LUA_OK;
		}

		this->is_sleeping = true;
		this->sleep_for_ms(PENALTY_SLEEP_MS);
	}
	return status;
}

bool ScriptThread::run(int arg_count) {
	// If a coroutine inside the thread was interrupted by lua_break(), that specific coroutine needs to be resumed
	// callback_debuginterrupt will be called, which will provide a pointer to the coroutine that needs to resume
	if (this->is_thread_stopped)
		return true;
	if (this->interrupted != nullptr) {
		lua_State *save_interrupted = this->interrupted;
		int interrupted_status = this->resume_script_thread_with_stopwatch(this->interrupted, 0);
		if (interrupted_status == LUA_BREAK) {
			return false;
		}

		if (save_interrupted == this->interrupted) {
			this->interrupted = nullptr;
		} else {
			throw std::runtime_error("Interrupt thread pointer was changed");
		}
	}
	if (this->is_thread_stopped)
		return true;

	// Try resuming the main coroutine this thread object is for
	int status = this->resume_script_thread_with_stopwatch(this->L, arg_count);

	if (status == 0) {
		int n = lua_gettop(this->L);
		if (n) {
			luaL_checkstack(this->L, LUA_MINSTACK, "too many results to print");
			lua_getglobal(this->L, "print");
			lua_insert(this->L, 1);  
			lua_pcall(this->L, n, 0, 0);
		}
	} else {
		std::string error;

		if (status == LUA_BREAK || status == LUA_YIELD) {
			return false; // Thread has not finished
		} else if (const char* str = lua_tostring(this->L, -1)) {
			error = str;
		}

		error += "\nstack backtrace:\n";
		error += lua_debugtrace(this->L);

		const char *c_str = error.c_str();
		this->send_message(VM_MESSAGE_SCRIPT_ERROR, 0, 0, c_str, strlen(c_str));
		//fprintf(stderr, "%s\n", error.c_str());
	}
	return true; // Thread finished and can be removed
}

void ScriptThread::stop() {
	this->was_preempted = false;
	if (!this->is_thread_stopped) {
		lua_resetthread(this->L);
		lua_unref(this->script->L, this->thread_reference);
		this->is_thread_stopped = true;
	}
}

void ScriptThread::sleep_for_ms(int ms) {
	if (ms == 0)
		return;
	this->is_sleeping = true;
	if (ms >= 500) {
		// If the delay is half a second or more, the program is probably behaving well, so the penalty timer should get reduced or reset
		unsigned long long subtract = ms * ONE_MILLISECOND_IN_NANOSECONDS / 2;
		if (this->nanoseconds >= subtract)
			this->nanoseconds -= subtract;
		else
			this->nanoseconds = 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &this->wake_up_at);
	unsigned long long desired_nanoseconds = this->wake_up_at.tv_nsec + ms * ONE_MILLISECOND_IN_NANOSECONDS;
	if (desired_nanoseconds > 999999999) {
		this->wake_up_at.tv_sec += desired_nanoseconds / ONE_SECOND_IN_NANOSECONDS;
	}
	this->wake_up_at.tv_nsec = desired_nanoseconds % ONE_SECOND_IN_NANOSECONDS;
}

void ScriptThread::send_message(VM_MessageType type, int other_id, unsigned char status, const void *data, size_t data_len) {
	send_outgoing_message(type, this->script->vm->user_id, this->script->entity_id, other_id, status, data, data_len);
}

void lua_c_function_parameter_check(lua_State *L, int param_count, const char *arguments) {
	int arg_count = lua_gettop(L);
	if (param_count > 0 && param_count != arg_count) {
		luaL_error(L, "Function needs exactly %d arguments (got %d)", param_count, arg_count);
	}
	if (param_count < 0 && arg_count < -param_count) {
		luaL_error(L, "Function needs at least %d arguments (got %d)", -param_count, arg_count);
	}

	// Check for proper parameters
	for (int i=0; i<abs(param_count); i++) {
		switch (arguments[i]) {
			case 'E': // Entity
				if (lua_type(L, i+1) == LUA_TNUMBER || lua_type(L, i+1) == LUA_TSTRING)
					continue;
				luaL_checktype(L, i+1, LUA_TTABLE); // Add a further check here?
				continue;
			case 'b': // Boolean
				luaL_checktype(L, i+1, LUA_TBOOLEAN);
				continue;
			case 's': // String
				luaL_checktype(L, i+1, LUA_TSTRING);
				continue;
			case 'n': // Number
				luaL_checktype(L, i+1, LUA_TNUMBER);
				continue;
			case 'i': // Integer
				luaL_checktype(L, i+1, LUA_TNUMBER);
				if (lua_tonumber(L, i+1) != lua_tointeger(L, i+1))
					luaL_typeerrorL(L, i+1, "integer");
				continue;
			case 'I': // String or integer
				if (lua_type(L, i+1) != LUA_TNUMBER && lua_type(L, i+1) != LUA_TSTRING)
					luaL_typeerrorL(L, i+1, "integer or string");
				continue;
			case 't': // Table
				luaL_checktype(L, i+1, LUA_TTABLE);
				continue;
			case 'F': // Function or nil
				if (lua_isnil(L, i+1)) 
					continue;
			case 'f': // Function
				luaL_checktype(L, i+1, LUA_TFUNCTION);
				continue;
			case '$':
				continue;
			default:
				break;
		}
	}
}

int ScriptThread::send_api_call(lua_State *L, const char *command_name, bool request_response, int param_count, const char *arguments) {
	int arg_count = lua_gettop(L);
	lua_c_function_parameter_check(L, param_count, arguments);

	unsigned char out_buffer[0x10000];
	unsigned char *write = out_buffer;
	unsigned char *buffer_end = out_buffer + sizeof(out_buffer);

	*(write++) = API_VALUE_STRING;
	*((int*)write) = strlen(command_name);
	write += 4;
	memcpy(write, command_name, strlen(command_name));
	write += strlen(command_name);

	int n;
	const char *s;
	size_t l;
	for (int i=0; i<arg_count; i++) {
		switch (arguments[i]) {
			case 0:
				i = arg_count; // Halt
				break;
			case 'E': // Entity
				if ((write + 5) >= buffer_end)
					return 0;
				if (lua_type(L, i+1) == LUA_TNUMBER) {
					goto do_integer;
				} else if (lua_type(L, i+1) == LUA_TSTRING) {
					goto do_string;
				} else if (lua_type(L, i+1) == LUA_TTABLE) {
					lua_getfield(L, i+1, "_id");
					*(write++) = API_VALUE_INTEGER;
					n = lua_tointeger(L, -1);
					*((int*)write) = n;
					write += 4;
					lua_pop(L, 1);
				}
				continue;
			case 'b': // Boolean
				if ((write + 1) >= buffer_end)
					return 0;
				*(write++) = API_VALUE_FALSE + lua_toboolean(L, i+1);
				continue;
			case '$':
			case 'I': // String or integer
			case 's': // String
			do_string:
				if ((write + 1) >= buffer_end)
					return 0;
				*(write++) = API_VALUE_STRING;
				s = luaL_tolstring(L, i+1, &l);
				*((int*)write) = l;
				if ((write + l + 4) >= buffer_end)
					return 0;
				write += 4;
				memcpy(write, s, l);
				lua_pop(L, 1);
				write += l;
				continue;
			case 'n': // Number
			case 'i': // Integer
			do_integer:
				if ((write + 5) >= buffer_end)
					return 0;
				*(write++) = API_VALUE_INTEGER;
				n = lua_tointeger(L, i+1);
				*((int*)write) = n;
				write += 4;
				continue;
			case 't': // Table
				if ((write + 1) >= buffer_end)
					return 0;
				*(write++) = API_VALUE_TABLE;
				// Convert to JSON?
				break;
			default:
				break;
		}
	}

	if (!request_response) {
		this->send_message(VM_MESSAGE_API_CALL, 0, arg_count+1, out_buffer, write - out_buffer);
		return 0;
	} else {
		this->is_waiting_for_api = true;
		time(&this->started_waiting_for_api_at);
		this->api_response_key = this->script->vm->next_api_result_key++;
		//fprintf(stderr, "Expecting response key %d\n", this->api_response_key);
		this->send_message(VM_MESSAGE_API_CALL_GET, this->api_response_key, arg_count+1, out_buffer, write - out_buffer);
		return lua_break(L);
	}
}
