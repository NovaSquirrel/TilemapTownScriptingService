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

// From https://stackoverflow.com/a/6749766
timespec diff(timespec start, timespec end) {
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
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
			thread->was_preempted = true;
			lua_break(L);
			return;
		}
	}
}

VM::VM() {
	puts("new VM");
	this->total_allocated_memory = 0;
	this->memory_allocation_limit = 2*1024*1024;

	this->L = lua_newstate(lua_allocator, this);
    luaL_openlibs(this->L);
	register_lua_api(this->L);
	luaL_sandbox(this->L);
	lua_setthreaddata(this->L, NULL);

	lua_Callbacks* cb = lua_callbacks(this->L);
	cb->userthread = callback_userthread;
//	cb->onallocate = callback_allocate;
	cb->interrupt = callback_interrupt;
//	cb->debugbreak = callback_break;
	cb->debuginterrupt = callback_debuginterrupt;
}

VM::~VM() {
	puts("del VM");
	lua_close(this->L);
}

void VM::add_script(int entity_id, const char *code) {
	Script *script = new Script(this, entity_id);
	script->compile_and_start(code);
	script->compile_and_start(code);
	script->compile_and_start(code);
	this->scripts.insert(std::unique_ptr<Script>(script) );
}

void VM::remove_script(int entity_id) {
	for (auto itr = this->scripts.begin(); itr != this->scripts.end(); ) {
		Script *script = (*itr).get();

		if (script->entity_id == entity_id) {
			itr = this->scripts.erase(itr);
		} else {
			++itr;
		}
	}
}

RunThreadsStatus VM::run_scripts() {
	printf("VM - running scripts - %ld\n", this->scripts.size());
	bool any_scripts_not_done;
	bool all_scripts_waiting;
	
	bool trying_again_after_clearing_scheduled_flag = false;

	if(this->scripts.empty())
		return RUN_THREADS_FINISHED;
	while (true) {
		any_scripts_not_done = false;
		all_scripts_waiting = true;
		bool any_scripts_newly_scheduled = false;

		for(auto itr = this->scripts.begin(); itr != this->scripts.end(); ++itr) {
			Script *script = (*itr).get();
			if (script->was_scheduled_yet) {
				printf("script %p already scheduled\n", script);
				continue;
			}
			printf("script %p NOT already scheduled\n", script);
			script->was_scheduled_yet = true;
			any_scripts_newly_scheduled = true;

			RunThreadsStatus status = script->run_threads();
			printf("Script status %d\n", status);

			switch (status) {
				case RUN_THREADS_FINISHED:
					break;
				case RUN_THREADS_KEEP_GOING:
					all_scripts_waiting = false;
				case RUN_THREADS_ALL_WAITING:
					any_scripts_not_done = true;
					break;
				case RUN_THREADS_PREEMPTED:
					puts("Preempted");
					return RUN_THREADS_PREEMPTED;
			}
		}
		
		if (trying_again_after_clearing_scheduled_flag)
			break;
		if (!any_scripts_newly_scheduled) { // Give everything another chance to run, because the entire list was marked as already scheduled
			puts("Trying again");
			trying_again_after_clearing_scheduled_flag = true;
			for(auto itr = this->scripts.begin(); itr != this->scripts.end(); ++itr) {
				(*itr).get()->was_scheduled_yet = false;
			}
		} else
			break;
	}

//	if (!any_scripts_not_done)
//		return RUN_THREADS_FINISHED;
//	if (all_scripts_waiting)
//		return RUN_THREADS_ALL_WAITING;
	return RUN_THREADS_KEEP_GOING;
}

///////////////////////////////////////////////////////////

Script::Script(VM *vm, int entity_id) {
	this->vm = vm;
	this->entity_id = entity_id;
	this->was_scheduled_yet = false;

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

bool Script::compile_and_start(const char *source) {
	size_t bytecodeSize = 0;

	char* bytecode = luau_compile(source, strlen(source), NULL, &bytecodeSize);
	//printf("Compiled; %ld bytes\n", bytecodeSize);

	int result = luau_load(this->L, "chunk", bytecode, bytecodeSize, 0);
	if (result) {
		fprintf(stderr, "Failed to load script: %s\n", lua_tostring(this->L, -1));
		exit(1);
	}

	// Try running the thread here, but if it doesn't finish then add it to a list to run later
	ScriptThread *thread = new ScriptThread(this);
	lua_xmove(this->L, thread->L, 1);
	if(thread->run()) {
		delete thread;
		return true;
	} else {
		this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
		return false;
	}
}

bool Script::start_callback() {
	// Try running the thread here, but if it doesn't finish then add it to a list to run later
	ScriptThread *thread = new ScriptThread(this);
	lua_xmove(this->L, thread->L, 2); // Should put function and data into the script first
	if(thread->run()) {
		delete thread;
		return true;
	} else {
		this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
		return false;
	}
}

RunThreadsStatus Script::run_threads() {
	printf("Script %p - running scripts - %ld\n", this, this->threads.size());

	bool any_threads_run = false;
	bool trying_again_after_clearing_scheduled_flag = false;

	if(this->threads.empty())
		return RUN_THREADS_FINISHED;
	while (true) {
		bool any_threads_newly_scheduled = false;

		for (auto itr = this->threads.begin(); itr != this->threads.end(); ) {
			ScriptThread *thread = (*itr).get();
			if (thread->was_scheduled_yet) {
				printf("\tthread %p already scheduled\n", thread);
				++itr;
				continue;
			}
			printf("\tthread %p NOT already scheduled\n", thread);
			thread->was_scheduled_yet = true;
			any_threads_newly_scheduled = true;

			// If thread is sleeping, check if it's time to wake up
			if(thread->is_sleeping) {
				struct timespec now_ts;
				clock_gettime(CLOCK_MONOTONIC, &now_ts);
				if (now_ts.tv_sec < thread->wake_up_at.tv_sec) {
					++itr;
					continue;
				}
				if (now_ts.tv_sec > thread->wake_up_at.tv_sec || (now_ts.tv_sec == thread->wake_up_at.tv_sec && now_ts.tv_nsec >= thread->wake_up_at.tv_nsec)) {
					thread->is_sleeping = false;
				} else {
					++itr;
					continue;
				}
			}

			// Thread is not sleeping, or has just woken up
			any_threads_run = true;
			if (thread->run()) {
				itr = this->threads.erase(itr);
			} else {
				if (thread->was_preempted) {
					puts("\tthread was preempted");
					return RUN_THREADS_PREEMPTED;
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
		return RUN_THREADS_FINISHED;
	if(!any_threads_run)
		return RUN_THREADS_ALL_WAITING;
	return RUN_THREADS_KEEP_GOING;
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
	this->total_nanoseconds = 0;
	this->is_sleeping = false;
	this->is_waiting_for_api = false;
	this->is_thread_stopped = false;
	this->was_scheduled_yet = false;
}

ScriptThread::~ScriptThread() {
	// Remove the reference to allow the thread to get garbage collected
	this->stop();
}

int ScriptThread::resume_script_thread_with_stopwatch(lua_State *state) {
	this->was_preempted = false;

	struct timespec start_ts, end_ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_ts);
	unsigned long long start_nanoseconds = start_ts.tv_sec * ONE_SECOND_IN_NANOSECONDS + start_ts.tv_nsec;
	unsigned long long halt_nanoseconds = start_nanoseconds + TIME_SLICE_IN_NANOSECONDS;
	this->preempt_at.tv_sec  = halt_nanoseconds / ONE_SECOND_IN_NANOSECONDS;
	this->preempt_at.tv_nsec = halt_nanoseconds % ONE_SECOND_IN_NANOSECONDS;

	int status = lua_resume(state, NULL, 0);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_ts);

	unsigned long long end_nanoseconds = end_ts.tv_sec * ONE_SECOND_IN_NANOSECONDS + end_ts.tv_nsec;
	unsigned long long nanoseconds = end_nanoseconds - start_nanoseconds;
	this->nanoseconds += nanoseconds;
	this->total_nanoseconds += nanoseconds;

	if(this->total_nanoseconds > TERMINATE_THREAD_AT_MS * ONE_MILLISECOND_IN_NANOSECONDS) {
		puts("Stopping a stuck thread");
		this->stop();
		return LUA_OK;
	}
	if(this->nanoseconds > PENALTY_THRESHOLD_MS * ONE_MILLISECOND_IN_NANOSECONDS) {
		puts("Making a thread sleep");
		this->is_sleeping = true;
		this->sleep_for_ms(PENALTY_SLEEP_MS);
	}
	return status;
}

bool ScriptThread::run() {
	// If a coroutine inside the thread was interrupted by lua_break(), that specific coroutine needs to be resumed
	// callback_debuginterrupt will be called, which will provide a pointer to the coroutine that needs to resume
	if (this->is_thread_stopped)
		return true;
	if (this->interrupted != nullptr) {
		lua_State *save_interrupted = this->interrupted;
		int interrupted_status = this->resume_script_thread_with_stopwatch(this->interrupted);
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
	int status = this->resume_script_thread_with_stopwatch(this->L);

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

		puts(error.c_str());
	}
	return true; // Thread finished and can be removed
}

void ScriptThread::stop() {
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
	if (ms >= 1000)
		this->nanoseconds = 0;

	clock_gettime(CLOCK_MONOTONIC, &this->wake_up_at);
	unsigned long long desired_nanoseconds = this->wake_up_at.tv_nsec + ms * ONE_MILLISECOND_IN_NANOSECONDS;
	if (desired_nanoseconds > 999999999) {
		this->wake_up_at.tv_sec += desired_nanoseconds / ONE_SECOND_IN_NANOSECONDS;
	}
	this->wake_up_at.tv_nsec = desired_nanoseconds % ONE_SECOND_IN_NANOSECONDS;
}
