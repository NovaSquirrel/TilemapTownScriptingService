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

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

class VM;
class Script;
class ScriptThread;

class VM {
	std::unordered_set <std::unique_ptr<Script>> scripts;

public:
	lua_State *L;
	size_t total_allocated_memory;
	size_t memory_allocation_limit;

	VM();
	~VM();
	friend class Script;
};

class Script {
	std::unordered_set <std::unique_ptr<ScriptThread>> threads;
	int thread_reference;
	VM *vm;

public:
	lua_State *L;
	bool compile_and_start(const char *code);
	bool start_callback();
	bool run_threads();

	Script(VM *vm);
	~Script();
	friend class ScriptThread;
};

class ScriptThread {
	int thread_reference;
	Script *script;

public:
	lua_State *interrupted;
	lua_State *L;
	bool run();

	ScriptThread(Script *script);
	~ScriptThread();
};

// Prototypes
void register_lua_api(lua_State* L);
