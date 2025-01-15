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
	//puts("allocate");
}

void callback_interrupt(lua_State* L, int gc) {
	//printf("interrupt %p\n", L);
}

VM::VM() {
	this->total_allocated_memory = 0;
	this->memory_allocation_limit = 2*1024*1024;

	this->L = lua_newstate(lua_allocator, this);
    luaL_openlibs(this->L);
	register_lua_api(this->L);
	luaL_sandbox(this->L);
	lua_setthreaddata(this->L, NULL);

	lua_Callbacks* cb = lua_callbacks(this->L);
	cb->userthread = callback_userthread;
	cb->onallocate = callback_allocate;
	cb->interrupt = callback_interrupt;
}

VM::~VM() {
	lua_close(this->L);
}

Script::Script(VM *vm) {
	this->vm = vm;

	// Create a thread and store a reference away to prevent it from getting garbage collected
	this->L = lua_newthread(vm->L);
	this->thread_reference = lua_ref(vm->L, -1);
	lua_pop(vm->L, 1);

	luaL_sandboxthread(this->L);
	lua_setthreaddata(this->L, this);
}

Script::~Script() {
	// Remove the reference to allow the thread to get garbage collected
	lua_unref(this->vm->L, this->thread_reference);

	lua_resetthread(this->L);
	lua_gc(this->L, LUA_GCCOLLECT, 0);
}

void Script::compile_and_start(const char *source) {
	size_t bytecodeSize = 0;

	char* bytecode = luau_compile(source, strlen(source), NULL, &bytecodeSize);
	printf("Compiled; %ld bytes\n", bytecodeSize);

	int result = luau_load(this->L, "chunk", bytecode, bytecodeSize, 0);
	if (result) {
		fprintf(stderr, "Failed to load script: %s\n", lua_tostring(this->L, -1));
		exit(1);
	}

	ScriptThread *thread = new ScriptThread(this);
	lua_xmove(this->L, thread->L, 1);
	if(thread->run()) {
		puts("Finished");
		delete thread;
	} else {
		puts("Yielded");
		this->threads.insert(std::unique_ptr<ScriptThread>(thread) );
	}
}

ScriptThread::ScriptThread(Script *s) {
	this->script = s;

	// Create a thread and store a reference away to prevent it from getting garbage collected
	this->L = lua_newthread(s->L);
	this->thread_reference = lua_ref(s->L, -1);
	lua_pop(s->L, 1);
}

ScriptThread::~ScriptThread() {
	// Remove the reference to allow the thread to get garbage collected
	lua_unref(this->script->L, this->thread_reference);

	lua_resetthread(this->L);
}

bool ScriptThread::run() {
	int status = lua_resume(this->L, NULL, 0);

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

		if (status == LUA_YIELD) {
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
