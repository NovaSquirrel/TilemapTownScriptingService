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

static void create_entity_table(lua_State* L, const char *ID, int ID_int) {
	lua_newtable(L);

	lua_newtable(L);
	lua_getglobal(L, "Entity");
	lua_setfield(L, -2, "__index");
	lua_setreadonly(L, -1, true);

	lua_setmetatable(L, -2);

	lua_pushstring(L, ID);
	lua_setfield(L, -2, "id");
	lua_pushinteger(L, ID_int);
	lua_setfield(L, -2, "_id");

	lua_setreadonly(L, -1, true);
}
static int tt_entity_new(lua_State* L) {
	return 0;
}
static int tt_entity_me(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		char id[20];
		if(thread->script->entity_id >= 0) {
			sprintf(id, "%d", thread->script->entity_id);
		} else {
			sprintf(id, "~%d", -thread->script->entity_id);
		}
		create_entity_table(L, id, thread->script->entity_id);
		return 1;
	}
	return 0;
}
static int tt_entity_get(lua_State* L) {
	const char *ID = luaL_checkstring(L, 1);
	if (ID) {
		if (ID[0] != '~')
			create_entity_table(L, ID, strtol(ID, nullptr, 10));
		else
			create_entity_table(L, ID, -strtol(ID+1, nullptr, 10));
		return 1;
	}
	return 0;
}
static int tt_map_who(lua_State* L) {
	return 0;
}
static int tt_map_turf_at(lua_State* L) {
//	int x = luaL_checkinteger(L, 1);
//	int y = luaL_checkinteger(L, 2);
	return 0;
}
static int tt_map_objs_at(lua_State* L) {
//	int x = luaL_checkinteger(L, 1);
//	int y = luaL_checkinteger(L, 2);
	return 0;
}
static int tt_map_dense_at(lua_State* L) {
//	int x = luaL_checkinteger(L, 1);
//	int y = luaL_checkinteger(L, 2);
	return 0;
}
static int tt_map_tile_info(lua_State* L) {
	if (lua_istable(L, 1)) {
		return 1;
	} else if (lua_isstring(L, 1)) {

	}
	return 0;
}
static int tt_map_map_info(lua_State* L) {
	return 0;
}
static int tt_map_within_map(lua_State* L) {
//	int x = luaL_checkinteger(L, 1);
//	int y = luaL_checkinteger(L, 2);
	return 0;
}
static int tt_map_add_callback(lua_State* L) {
	return 0;
}
static int tt_map_remove_callback(lua_State* L) {
//	int id = luaL_checkinteger(L, 1);
	return 0;
}
static int tt_map_reset_callbacks(lua_State* L) {
	return 0;
}
static int tt_storage_reset(lua_State* L) {
//	const char* key = luaL_checkstring(L, 1);
	return 0;
}
static int tt_storage_load(lua_State* L) {
//	const char* key = luaL_checkstring(L, 1);
	return 0;
}
static int tt_storage_save(lua_State* L) {
//	const char* key = luaL_checkstring(L, 1);
	return 0;
}
static int tt_storage_list(lua_State* L) {
//	const char* key = luaL_checkstring(L, 1);
	return 0;
}
static int tt_mini_tilemap_new(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_tile(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_new(lua_State* L) {
	return 0;
}
static int tt_bitmap_sprite_new(lua_State* L) {
	return 0;
}
static int tt_tt_sleep(lua_State* L) {
	int delay_ms = luaL_checkinteger(L, 1);
	if (delay_ms == 0)
		return 0;
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		thread->sleep_for_ms(delay_ms);
	}
	return lua_break(L);
}
static int tt_tt_memory_used(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		lua_pushunsigned(L, thread->script->vm->total_allocated_memory);
		return 1;
	}
	lua_pushnil(L);
	return 1;
}
static int tt_tt_memory_free(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		lua_pushunsigned(L, thread->script->vm->memory_allocation_limit - thread->script->vm->total_allocated_memory);
		return 1;
	}
	lua_pushnil(L);
	return 1;
}
static int tt_tt_add_callback(lua_State* L) {
	return 0;
}
static int tt_tt_remove_callback(lua_State* L) {
	return 0;
}
static int tt_tt_reset_callbacks(lua_State* L) {
	return 0;
}
static int tt_entity_object_who(lua_State* L) {
	return 0;
}
static int tt_entity_object_move(lua_State* L) {
	return 0;
}
static int tt_entity_object_turn(lua_State* L) {
	return 0;
}
static int tt_entity_object_step(lua_State* L) {
	return 0;
}
static int tt_entity_object_say(lua_State* L) {
	return 0;
}
static int tt_entity_object_command(lua_State* L) {
	return 0;
}
static int tt_entity_object_tell(lua_State* L) {
	return 0;
}
static int tt_entity_object_typing(lua_State* L) {
	return 0;
}
static int tt_entity_object_set(lua_State* L) {
	return 0;
}
static int tt_entity_object_clone(lua_State* L) {
	return 0;
}
static int tt_entity_object_delete(lua_State* L) {
	return 0;
}
static int tt_entity_object_add_callback(lua_State* L) {
	return 0;
}
static int tt_entity_object_remove_callback(lua_State* L) {
	return 0;
}
static int tt_entity_object_reset_callbacks(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_resize(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_put(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_get(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_clear(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_rectfill(lua_State* L) {
	return 0;
}
static int tt_mini_tilemap_object_scroll(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_resize(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_put(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_get(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_clear(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_rectfill(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_rect(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_line(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_scroll(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_draw_sprite(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_draw_sprite_on(lua_State* L) {
	return 0;
}
static int tt_bitmap_2x4_object_draw_sprite_off(lua_State* L) {
	return 0;
}

// --------------------------------------------------------

void register_lua_api(lua_State* L) {
    static const luaL_Reg entity_funcs[] = {
		{"new", tt_entity_new},
		{"me",  tt_entity_me},
		{"get", tt_entity_get},
        {NULL, NULL},
    };

    static const luaL_Reg map_funcs[] = {
		{"who",             tt_map_who},
		{"turf_at",         tt_map_turf_at},
		{"objs_at",         tt_map_objs_at},
		{"dense_at",        tt_map_dense_at},
		{"tile_info",       tt_map_tile_info},
		{"map_info",        tt_map_map_info},
		{"within_map",      tt_map_within_map},
		{"add_callback",    tt_map_add_callback},
		{"remove_callback", tt_map_remove_callback},
		{"reset_callbacks", tt_map_reset_callbacks},
        {NULL, NULL},
    };

    static const luaL_Reg storage_funcs[] = {
		{"reset",      tt_storage_reset},
		{"load",       tt_storage_load},
		{"save",       tt_storage_save},
		{"list",       tt_storage_list},
        {NULL, NULL},
    };

    static const luaL_Reg mini_tilemap_funcs[] = {
		{"new",        tt_mini_tilemap_new},
		{"tile",       tt_mini_tilemap_tile},
        {NULL, NULL},
    };

    static const luaL_Reg bitmap_2x4_funcs[] = {
		{"new",        tt_bitmap_2x4_new},
        {NULL, NULL},
    };

    static const luaL_Reg bitmap_sprite_funcs[] = {
		{"new",        tt_bitmap_sprite_new},
        {NULL, NULL},
    };

    static const luaL_Reg misc_funcs[] = {
		{"sleep",           tt_tt_sleep},
		{"memory_used",     tt_tt_memory_used},
		{"memory_free",     tt_tt_memory_free},
		{"add_callback",    tt_tt_add_callback},
		{"remove_callback", tt_tt_remove_callback},
		{"reset_callbacks", tt_tt_reset_callbacks},
        {NULL, NULL},
    };

    static const luaL_Reg entity_object_funcs[] = {
		{"who",             tt_entity_object_who},
		{"move",            tt_entity_object_move},
		{"turn",            tt_entity_object_turn},
		{"step",            tt_entity_object_step},
		{"say",             tt_entity_object_say},
		{"command",         tt_entity_object_command},
		{"tell",            tt_entity_object_tell},
		{"typing",          tt_entity_object_typing},
		{"set",             tt_entity_object_set},
		{"clone",           tt_entity_object_clone},
		{"delete",          tt_entity_object_delete},
		{"add_callback",    tt_entity_object_add_callback},
		{"remove_callback", tt_entity_object_remove_callback},
		{"reset_callbacks", tt_entity_object_reset_callbacks},
        {NULL, NULL},
    };

    static const luaL_Reg mini_tilemap_object_funcs[] = {
		{"resize",     tt_mini_tilemap_object_resize},
		{"put",        tt_mini_tilemap_object_put},
		{"get",        tt_mini_tilemap_object_get},
		{"clear",      tt_mini_tilemap_object_clear},
		{"rectfill",   tt_mini_tilemap_object_rectfill},
		{"scroll",     tt_mini_tilemap_object_scroll},
        {NULL, NULL},
    };

    static const luaL_Reg bitmap_2x4_object_funcs[] = {
		{"resize",          tt_bitmap_2x4_object_resize},
		{"put",             tt_bitmap_2x4_object_put},
		{"get",             tt_bitmap_2x4_object_get},
		{"clear",           tt_bitmap_2x4_object_clear},
		{"rectfill",        tt_bitmap_2x4_object_rectfill},
		{"rect",            tt_bitmap_2x4_object_rect},
		{"line",            tt_bitmap_2x4_object_line},
		{"scroll",          tt_bitmap_2x4_object_scroll},
		{"draw_sprite",     tt_bitmap_2x4_object_draw_sprite},
		{"draw_sprite_on",  tt_bitmap_2x4_object_draw_sprite_on},
		{"draw_sprite_off", tt_bitmap_2x4_object_draw_sprite_off},
        {NULL, NULL},
    };

    lua_pushvalue(L, LUA_GLOBALSINDEX);

    luaL_register(L, "entity",        entity_funcs);
    luaL_register(L, "map",           map_funcs);
    luaL_register(L, "storage",       storage_funcs);
    luaL_register(L, "mini_tilemap",  mini_tilemap_funcs);
    luaL_register(L, "bitmap_2x4",    bitmap_2x4_funcs);
    luaL_register(L, "bitmap_sprite", bitmap_sprite_funcs);
    luaL_register(L, "tt",            misc_funcs);

    luaL_register(L, "Entity",        entity_object_funcs);
    luaL_register(L, "MiniTilemap",   mini_tilemap_object_funcs);
    luaL_register(L, "Bitmap2x4",     bitmap_2x4_object_funcs);

    lua_pop(L, 1);
}
