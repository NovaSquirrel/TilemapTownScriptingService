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

#include <queue>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <future>
#include <atomic>
#include <mutex>

#define ONE_SECOND_IN_NANOSECONDS 1000000000ULL
#define ONE_MILLISECOND_IN_NANOSECONDS 1000000ULL
#define TIME_SLICE_IN_NANOSECONDS (ONE_MILLISECOND_IN_NANOSECONDS * 10)

#define PENALTY_THRESHOLD_MS 500
#define PENALTY_SLEEP_MS 2500
#define TERMINATE_THREAD_AT_MS 2000

class VM;
class Script;
class ScriptThread;

enum RunThreadsStatus {
	RUN_THREADS_FINISHED,
	RUN_THREADS_KEEP_GOING,
	RUN_THREADS_ALL_WAITING,
	RUN_THREADS_PREEMPTED,
};

///////////////////////////////////////////////////////////

enum VM_MessageType {
	VM_MESSAGE_START_SCRIPT,
	VM_MESSAGE_STOP_SCRIPT,
	VM_MESSAGE_API_RESULT,
	VM_MESSAGE_CALLBACK,
};

struct VM_Message {
	VM_MessageType type;
	time_t received_at;     // To allow messages to expire
	unsigned int user_id;   // VM ID
	unsigned int entity_id; // Script ID
	unsigned int other_id;  // Callback IDs, API result keys
	int variable;           // Miscellaneous use
	size_t data_len;
	void *data;
};

///////////////////////////////////////////////////////////

class VM {
	std::unordered_set <std::unique_ptr<Script>> scripts;
	lua_State *L;

public:
	int user_id;                    // User that this VM belongs to

	size_t total_allocated_memory;  // Amount of bytes this VM is currently using
	size_t memory_allocation_limit; // Maximum number of bytes this VM is allowed to use

	// Thread communication
	std::promise<VM_Message> incoming_message_promise;
	std::future<VM_Message> incoming_message_future;
	std::atomic_bool have_incoming_message;
	std::mutex incoming_message_mutex;

	std::unordered_map<int, VM_Message> api_results;

	bool is_any_script_sleeping;    // Are any script sleeping?
	timespec earliest_wake_up_at;   // If any scripts are sleeping, earliest time any of them will wake up

	void receive_message(VM_MessageType type, unsigned int entity_id, unsigned int other_id, int variable, size_t data_len, void *data);
	void add_script(int entity_id, const char *code);
	void remove_script(int entity_id);
	RunThreadsStatus run_scripts();

	VM(int user_id);
	~VM();
	friend class Script;
};

class Script {
	int thread_reference;         // Used with lua_ref() to store a reference to the thread, and prevent it from being garbage collected
	bool was_scheduled_yet;
	std::unordered_set <std::unique_ptr<ScriptThread>> threads;
	lua_State *L;

public:
	int entity_id;                // Entity that this script controls (if negative, it's temporary)

	bool is_any_thread_sleeping;  // Is any thread currently sleeping?
	timespec earliest_wake_up_at; // If any thread is sleeping, then it's the earliest time any of them will wake up

	VM *vm;                       // VM containing the script's own global table and all of its threads

	bool compile_and_start(const char *code);
	bool start_callback();
	RunThreadsStatus run_threads();

	Script(VM *vm, int entity_id);
	~Script();
	friend class ScriptThread;
	friend class VM;
};

///////////////////////////////////////////////////////////

class ScriptThread {
	int thread_reference;      // Used with lua_ref() to store a reference to the thread, and prevent it from being garbage collected
	bool was_scheduled_yet;    // Thread got a chance to run
	unsigned int nanoseconds;             // Amount of nanoseconds this thread has been running for (to apply penalty)
	unsigned long long total_nanoseconds; // Amount of nanoseconds this thread has been running for (to just force stop the script)
	bool is_thread_stopped;    // Thread was forcibly stopped
	lua_State *L;              // This thread's state

public:
	bool is_sleeping;          // Currently sleeping
	timespec wake_up_at;       // If sleeping, when to wake up

	bool is_waiting_for_api;   // Currently waiting for a response from the Tilemap Town server
	bool have_api_response;    // Tilemap Town server has a response waiting for this thread
	int api_response_key;      // Key for knowing that API responses are for this thread specifically

	timespec preempt_at;       // When to pause the thread and let another thread run
	bool was_preempted;        // Was the thread stopped because it ran too long?

	Script *script;            // Script this thread belongs to
	lua_State *interrupted;    // Set by the "debuginterrupt" callback

	bool run();                // Returns true if the thread has completed
	int resume_script_thread_with_stopwatch(lua_State *state);
	void sleep_for_ms(int ms);
	void stop();
	void send_message(VM_MessageType type, unsigned int other_id, int variable, size_t data_len, void *data);

	ScriptThread(Script *script);
	~ScriptThread();
	friend class Script;
};

///////////////////////////////////////////////////////////

// Prototypes
void register_lua_api(lua_State* L);
void set_timespec_now_plus_ms(struct timespec &ts, unsigned long ms);
bool is_ts_earlier(timespec now, timespec future);

///////////////////////////////////////////////////////////

// Global variables
extern std::mutex outgoing_messages_mtx;
extern std::condition_variable outgoing_messages_cv;
extern std::queue<VM_Message> outgoing_messages;
extern std::atomic_bool have_outgoing_message;
