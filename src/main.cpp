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

void test(VM *l) {
	Script s = Script(l);

	s.run("eee = {}; for i = 1,10 do eee[i] = i; print(i) end");
	s.run("print(1234);");
	s.run("co = coroutine.create(function () print(\"hi\") end); coroutine.resume(co);");

	Script s2 = Script(l);
	s2.run("print(999);");
}

int main(void) {
	VM l = VM();
	test(&l);
}
