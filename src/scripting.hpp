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

#include <luacode.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

#define ONE_SECOND_IN_NANOSECONDS 1000000000

class VM;
class Script;
class ScriptThread;

enum RunThreadsStatus {
	RUN_THREADS_FINISHED,
	RUN_THREADS_KEEP_GOING,
	RUN_THREADS_ALL_WAITING,
};

///////////////////////////////////////////////////////////

class VM {
	std::unordered_set <std::unique_ptr<Script>> scripts;

public:
	lua_State *L;                   //
	size_t total_allocated_memory;  // Amount of bytes this VM is currently using
	size_t memory_allocation_limit; // Maximum number of bytes this VM is allowed to use

	VM();
	~VM();
	friend class Script;
};

class Script {
	int thread_reference;      // Used with lua_ref() to store a reference to the thread, and prevent it from being garbage collected
	std::unordered_set <std::unique_ptr<ScriptThread>> threads;

public:
	int entity_id;             // Entity that this script controls (if negative, it's temporary)

	VM *vm;                    // VM containing the script's own global table and all of its threads
	lua_State *L;
	bool compile_and_start(const char *code);
	bool start_callback();
	RunThreadsStatus run_threads();

	Script(VM *vm, int entity_id);
	~Script();
	friend class ScriptThread;
};

///////////////////////////////////////////////////////////

class ScriptThread {
	int thread_reference;      // Used with lua_ref() to store a reference to the thread, and prevent it from being garbage collected

public:
	bool is_sleeping;          // Currently sleeping
	timespec wake_up_at;       // If sleeping, when to wake up

	bool is_waiting_for_api;   // Currently waiting for a response from the Tilemap Town server
	bool have_api_response;    // Tilemap Town server has a response waiting for this thread
	int api_response_key;      // Key for knowing that API responses are for this thread specifically

	timespec halt_at;          // When to halt the thread
	bool was_halted;           // Was the thread stopped because it ran too long?
	unsigned int nanoseconds;  // Amount of nanoseconds this thread has been running for

	Script *script;            // Script this thread belongs to
	lua_State *interrupted;    // Set by the "debuginterrupt" callback
	lua_State *L;              // This thread's state

	bool run();                // Returns true if the thread has completed
	void sleep_for_ms(int ms);

	ScriptThread(Script *script);
	~ScriptThread();
};

// Prototypes
void register_lua_api(lua_State* L);
void set_timespec_now_plus_ms(struct timespec &ts, unsigned long ms);
