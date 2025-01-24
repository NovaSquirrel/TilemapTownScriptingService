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
#include <queue>

#define ONE_SECOND_IN_NANOSECONDS 1000000000ULL
#define ONE_MILLISECOND_IN_NANOSECONDS 1000000ULL
#define TIME_SLICE_IN_NANOSECONDS (ONE_MILLISECOND_IN_NANOSECONDS * 10)

#define PENALTY_THRESHOLD_MS 500
#define PENALTY_SLEEP_MS 2500
#define TERMINATE_THREAD_AT_MS 2000
#define API_RESULT_TIMEOUT_IN_SECONDS 30

#define MESSAGE_HEADER_SIZE (4*3+1)

class VM;
class Script;
class ScriptThread;

enum RunThreadsStatus {
	RUN_THREADS_FINISHED,
	RUN_THREADS_KEEP_GOING,
	RUN_THREADS_ALL_WAITING,
	RUN_THREADS_PREEMPTED,
};

enum CallbackTypeID {
	CALLBACK_MISC_SHUTDOWN,
	CALLBACK_MAP_JOIN,
	CALLBACK_MAP_LEAVE,
	CALLBACK_MAP_CHAT,
	CALLBACK_MAP_BUMP,
	CALLBACK_SELF_PRIVATE_MESSAGE,
	CALLBACK_SELF_GOT_PERMISSION,
	CALLBACK_SELF_TOOK_CONTROLS,
	CALLBACK_SELF_KEY_PRESS,
	CALLBACK_SELF_CLICK,
	CALLBACK_SELF_BOT_COMMAND_BUTTON,
	CALLBACK_SELF_REQUEST_RECEIVED,
	CALLBACK_SELF_USE,
	CALLBACK_SELF_SWITCH_MAP,
	CALLBACK_COUNT,
	CALLBACK_INVALID,
};

///////////////////////////////////////////////////////////

enum VM_MessageType {
	VM_MESSAGE_PING,          // No arguments (still includes a length count of zero)
	VM_MESSAGE_PONG,          // No arguments (still includes a length count of zero)
	VM_MESSAGE_VERSION_CHECK, // User ID = 0, Entity ID = 0, Other = Version number
	VM_MESSAGE_SHUTDOWN,      // No arguments (still includes a length count of zero)
	VM_MESSAGE_START_SCRIPT,  // User ID, Entity ID, Other = Source code ID, Status = 0
	VM_MESSAGE_RUN_CODE,      // User ID, Entity ID, Other = Source code ID, Status = 0  | Data = code to run 
	VM_MESSAGE_STOP_SCRIPT,   // User ID, Entity ID, Other = 0, Status = 0
	VM_MESSAGE_API_CALL,      // User ID, Entity ID, Other = API result key, Status = argument/result count | Data = data given or returned (or it may be in the status) - Does not request a response
	VM_MESSAGE_API_CALL_GET,  // User ID, Entity ID, Other = API result key, Status = argument/result count | Data = data given or returned (or it may be in the status) - Requests that information be returned
	VM_MESSAGE_CALLBACK,      // User ID, Entity ID, Other = Callback type, Status = argument/result count | Data = callback data, in the same format as the API call
	VM_MESSAGE_SET_CALLBACK,  // User ID, Entity ID, Other = Callback type, Status = 1 to turn it on, 0 to turn it off
};

enum API_Value_Type {
	API_VALUE_NIL,
	API_VALUE_FALSE,
	API_VALUE_TRUE,
	API_VALUE_INTEGER,
	API_VALUE_STRING,
	API_VALUE_JSON,
	API_VALUE_TABLE,
};

/*
When sent over a pipe, this is formatted as:
TT LL-LL-LL UU-UU-UU-UU EE-EE-EE-EE OO-OO-OO-OO SS [rest of it is the arbitrary data]
Length includes the entire rest of the message after the length number and type number
All numbers are little endian
*/
struct VM_Message {
	VM_MessageType type;
	time_t received_at;     // To allow messages to expire
	int user_id;            // VM ID
	int entity_id;          // Script ID
	int other_id;           // Callback IDs, API result keys
	unsigned char status;   // Miscellaneous use
	size_t data_len;
	void *data;
};

///////////////////////////////////////////////////////////

class VM {
	std::unordered_map<int, std::unique_ptr<Script>> scripts;
	lua_State *L;

public:
	int user_id;                    // User that this VM belongs to
	std::thread thread;

	size_t total_allocated_memory;  // Amount of bytes this VM is currently using
	size_t memory_allocation_limit; // Maximum number of bytes this VM is allowed to use

	// Thread communication
	std::promise<void> incoming_message_promise;
	std::future<void> incoming_message_future;
	std::mutex incoming_message_mutex; // Lock this before modifying "have_incoming_message" or "incoming_messages"
	std::atomic_bool have_incoming_message;
	std::queue<VM_Message> incoming_messages;

	std::unordered_map<int, VM_Message> api_results;
	int next_api_result_key;

	bool is_any_script_sleeping;    // Are any script sleeping?
	timespec earliest_wake_up_at;   // If any scripts are sleeping, earliest time any of them will wake up

	void receive_message(VM_MessageType type, int entity_id, int other_id, unsigned char status, void *data, size_t data_len);
	void send_message(VM_MessageType type, int other_id, unsigned char status, void *data, size_t data_len);
	void add_script(int entity_id);
	void run_code_on_self(const char *bytecode, size_t bytecode_size);
	void run_code_on_script(int entity_id, const char *code, size_t code_size);
	void remove_script(int entity_id);
	void thread_function();
	void start_thread();
	void stop_thread();
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
	int callback_ref[CALLBACK_COUNT];

	bool is_any_thread_sleeping;  // Is any thread currently sleeping?
	timespec earliest_wake_up_at; // If any thread is sleeping, then it's the earliest time any of them will wake up

	VM *vm;                       // VM containing the script's own global table and all of its threads

	bool compile_and_start(const char *source, size_t source_len);
	bool start_callback(int callback_id, int data_item_count, void *data, size_t data_len);
	RunThreadsStatus run_threads();
	bool shutdown();

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
	time_t started_waiting_for_api_at; // When the thread starting waiting for the API
	int api_response_key;      // Key for knowing that API responses are for this thread specifically

	timespec preempt_at;       // When to pause the thread and let another thread run
	bool was_preempted;        // Was the thread stopped because it ran too long?

	Script *script;            // Script this thread belongs to
	lua_State *interrupted;    // Set by the "debuginterrupt" callback

	bool run(int arg_count);   // Returns true if the thread has completed
	int resume_script_thread_with_stopwatch(lua_State *state, int arg_count);
	void sleep_for_ms(int ms);
	void stop();
	void send_message(VM_MessageType type, int other_id, unsigned char status, void *data, size_t data_len);
	int send_api_call(lua_State *L, const char *command_name, bool request_response, int param_count, const char *arguments);

	ScriptThread(Script *script);
	~ScriptThread();
	friend class Script;
};

///////////////////////////////////////////////////////////

// Prototypes
void register_lua_api(lua_State* L);
void set_timespec_now_plus_ms(struct timespec &ts, unsigned long ms);
bool is_ts_earlier(timespec now, timespec future);
int push_values_from_message_data(lua_State *L, int num_values, char *data, size_t data_len);
void lua_c_function_parameter_check(lua_State *L, int param_count, const char *arguments);

///////////////////////////////////////////////////////////

// Global variables
extern std::mutex outgoing_messages_mtx;
extern std::condition_variable outgoing_messages_cv;
extern std::queue<VM_Message> outgoing_messages;
extern std::atomic_bool have_outgoing_message;
