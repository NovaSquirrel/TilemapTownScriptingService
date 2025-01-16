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
#include<unistd.h>

void test(VM *l) {
	Script s = Script(l, 5);

	s.compile_and_start("eee = {}; for i = 1,10 do eee[i] = i; print(i) end");
	s.compile_and_start("print(1234);");
	s.compile_and_start("print(\"sleep test\"); tt.sleep(5000); print(\"sleep test 2\"); tt.sleep(5000); print(\"sleep test 3\");");

//	s.compile_and_start("co = coroutine.create(function () print(\"hi\"); coroutine.yield(222); tt.sleep(); print(\"hi 2\"); coroutine.yield(999); end);\n"
//	"while true do\na, b = coroutine.resume(co)\nprint(\"Got this:\", a, b)\nif a == false then break end end\n");

/*
	s.compile_and_start("function proxy()\n"
	"local co = coroutine.create(function() print(\"coroutine inside proxy\"); tt.sleep(); tt.sleep(); tt.sleep(); print(\"coroutine inside proxy 2\"); coroutine.yield(12345); end)\n"
	"local success, data = coroutine.resume(co)\n"
	"return data\n"
	"end\n"
	"\n"
	"print(\"Taco\")\n"
	"print(\"proxy got \"..proxy())\n"
	"\n"
	"\n"
	"\n"
	"local eee = coroutine.create(function() print(\"nested coroutine\"); print(proxy()); print(\"continued nested coroutine\"); coroutine.yield(123); end)\n"
	"print(coroutine.resume(eee))\n"
	"tt.sleep()\n"
	"tt.sleep()\n"
	"print(\"eepy\")\n"
	);
*/
//	s.compile_and_start("print(tt.memory_used()); print(tt.memory_free()); local eee = entity.me(); print(eee.id);");


	s.compile_and_start("print(\"this will hang!\"); while true do end; print(\"oops\")");

	int limit = 0;
	RunThreadsStatus status;
	do {
		status = s.run_threads();
		if(status == RUN_THREADS_ALL_WAITING) {
			puts("All threads are waiting");
			sleep(1);
//			usleep(100000);
		}
		limit++;
	} while(limit < 100 && status != RUN_THREADS_FINISHED);
}

int main(void) {
	VM l = VM();
	test(&l);
}
