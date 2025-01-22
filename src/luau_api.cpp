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
#include "cJSON.h"

void push_json_value(lua_State* L, cJSON *value) {
	switch (value->type) {
		case cJSON_False:
			lua_pushboolean(L, 0);
			break;
		case cJSON_True:
			lua_pushboolean(L, 1);
			break;
		default:
		case cJSON_NULL:
			lua_pushnil(L);
			break;
		case cJSON_Number:
			lua_pushnumber(L, value->valuedouble);
			break;
		case cJSON_String:
			lua_pushstring(L, value->valuestring);
			break;
		case cJSON_Array:
		{
			lua_createtable(L, cJSON_GetArraySize(value->child), 0);
			int index = 1;
			for (cJSON *item = value->child; item; item = item->next) {
				push_json_value(L, item);
				lua_rawseti(L, -2, index++);
			}
			break;
		}
		case cJSON_Object:
		{
			lua_createtable(L, 0, cJSON_GetArraySize(value->child));
			for (cJSON *item = value->child; item; item = item->next) {
				lua_pushstring(L, item->string);
				push_json_value(L, item);
				lua_rawset(L, -3);
			}
			break;
		}
	}
}

void push_json_data(lua_State* L, const char *string, size_t size) {
	cJSON *root = cJSON_ParseWithLength(string, size);
	if (!root) {
		lua_pushnil(L);
	}
	push_json_value(L, root);
	cJSON_Delete(root);
}

static int tt_tt_decode_json(lua_State* L) {
	size_t length;
	const char *string = lua_tolstring(L, 1, &length);
	if (string) {
		push_json_data(L, string, length);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

///////////////////////////////////////////////////////////

struct callback_name_lookup_item {
	const char *name;
	enum CallbackTypeID type;
};

struct callback_name_lookup_item callback_names_self[] = {
	{"private_message",    CALLBACK_SELF_PRIVATE_MESSAGE},
	{"got_permission",     CALLBACK_SELF_GOT_PERMISSION},
	{"took_controls",      CALLBACK_SELF_TOOK_CONTROLS},
	{"key_press",          CALLBACK_SELF_KEY_PRESS},
	{"click",              CALLBACK_SELF_CLICK},
	{"bot_command_button", CALLBACK_SELF_BOT_COMMAND_BUTTON},
	{"request_received",   CALLBACK_SELF_REQUEST_RECEIVED},
	{"use",                CALLBACK_SELF_USE},
	{NULL}
};
struct callback_name_lookup_item callback_names_map[] = {
	{"join",               CALLBACK_MAP_JOIN},
	{"leave",              CALLBACK_MAP_LEAVE},
	{"chat",               CALLBACK_MAP_CHAT},
	{"bump",               CALLBACK_MAP_BUMP},
	{NULL}
};
struct callback_name_lookup_item callback_names_misc[] = {
	{"shutdown",           CALLBACK_MISC_SHUTDOWN},
	{NULL}
};
CallbackTypeID get_callback_id_from_name(const char *name, struct callback_name_lookup_item *table) {
	for (int i = 0; table[i].name; i++)
		if (!strcmp(table[i].name, name))
			return table[i].type;
	return CALLBACK_INVALID;
}

///////////////////////////////////////////////////////////

static int tt_custom_print(lua_State* L) {
	// Copied from luaB_print in the Luau code
    int n = lua_gettop(L); // number of arguments
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al
        if (i > 1)
			fprintf(stderr, "\t");
		fprintf(stderr, "%s", s);
        lua_pop(L, 1); // pop result
    }
	fprintf(stderr, "\n");
	return 0;
}

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
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_new", true, 0, "t");
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
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_who", true, 0, "");
	return 0;
}
static int tt_map_turf_at(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_turf", true, 2, "ii");
	return 0;
}
static int tt_map_objs_at(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_objs", true, 2, "ii");
	return 0;
}
static int tt_map_dense_at(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_dense", true, -2, "iii");
	return 0;
}
static int tt_map_tile_lookup(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_tilelookup", true, 1, "s");
	return 0;
}
static int tt_map_map_info(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_info", true, 0, "");
	return 0;
}
static int tt_map_within_map(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_within", true, 2, "ii");
	return 0;
}
static int tt_map_set_callback(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (!thread)
		return 0;
	lua_c_function_parameter_check(L, 2, "Fs");
	CallbackTypeID callback_type = get_callback_id_from_name(luaL_checkstring(L, 2), callback_names_map);
	if (callback_type == CALLBACK_INVALID)
		return 0;
	if (thread->script->callback_ref[callback_type] != LUA_NOREF) {
		lua_unref(L, thread->script->callback_ref[callback_type]);
		thread->script->callback_ref[callback_type] = LUA_NOREF;
	}
	thread->send_message(VM_MESSAGE_SET_CALLBACK, callback_type, !lua_isnil(L, 1), nullptr, 0);
	if (!lua_isnil(L, 1)) {
		thread->script->callback_ref[callback_type] = lua_ref(L, 1);
	}
	return 0;
}
static int tt_storage_reset(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "s_reset", false, 0, "s");
	return 0;
}
static int tt_storage_load(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "s_load", true, 1, "s");
	return 0;
}
static int tt_storage_save(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "s_save", false, -2, "s.");
	return 0;
}
static int tt_storage_list(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "s_list", true, 0, "s");
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
static int tt_tt_owner_say(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "ownersay", false, 1, "s");
	return 0;
}

int push_values_from_message_data(lua_State *L, int num_values, char *data, size_t data_len) {
	if (!data)
		return 0;
	void *original_data = (void*)data;

	// Push the data contained in the message
	const char *data_end = (const char *)data + data_len;
	int values_pushed = 0;

	while (num_values && data < data_end) {
		int type = *(data++);
		int n;

		switch (type) {
			case API_VALUE_NIL:
				lua_pushnil(L);
				break;
			case API_VALUE_FALSE:
				lua_pushboolean(L, 0);
				break;
			case API_VALUE_TRUE:
				lua_pushboolean(L, 1);
				break;
			case API_VALUE_INTEGER:
				lua_pushinteger(L, *(int*)data);
				data += 4;
				break;
			case API_VALUE_STRING:
				n = *(int*)data;
				data += 4;
				lua_pushlstring(L, data, n);
				data += n;
				break;
			case API_VALUE_JSON:
				n = *(int*)data;
				data += 4;
				push_json_data(L, data, n);
				data += n;
				break;
			case API_VALUE_TABLE:
				break;
			default:
				break;
		}

		values_pushed++;
		num_values--;
	}

	// Clean up
	free(original_data);
	return values_pushed;
}

static int tt_tt_get_result(lua_State *L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		int key = thread->api_response_key;
		auto it = thread->script->vm->api_results.find(key);
		if(it != thread->script->vm->api_results.end()) {
			VM_Message message = (*it).second;
			thread->script->vm->api_results.erase(it);
			return push_values_from_message_data(L, message.status, (char*)message.data, message.data_len);
		} else {
			return 0;
		}
	}
	return 0;
}
static int tt_tt_memory_used(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		lua_pushunsigned(L, thread->script->vm->total_allocated_memory);
		return 1;
	}
	return 0;
}
static int tt_tt_memory_free(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		lua_pushunsigned(L, thread->script->vm->memory_allocation_limit - thread->script->vm->total_allocated_memory);
		return 1;
	}
	return 0;
}
static int tt_tt_set_callback(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (!thread)
		return 0;
	lua_c_function_parameter_check(L, 2, "Fs");
	CallbackTypeID callback_type = get_callback_id_from_name(luaL_checkstring(L, 2), callback_names_misc);
	if (callback_type == CALLBACK_INVALID)
		return 0;
	if (thread->script->callback_ref[callback_type] != LUA_NOREF) {
		lua_unref(L, thread->script->callback_ref[callback_type]);
		thread->script->callback_ref[callback_type] = LUA_NOREF;
	}
	thread->send_message(VM_MESSAGE_SET_CALLBACK, callback_type, !lua_isnil(L, 1), nullptr, 0);
	if (!lua_isnil(L, 1)) {
		thread->script->callback_ref[callback_type] = lua_ref(L, 1);
	}
	return 0;
}
static int tt_entity_object_who(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_who", true, 1, "E");
	return 0;
}
static int tt_entity_object_move(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_move", false, -2, "Eiii");
	return 0;
}
static int tt_entity_object_turn(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_turn", false, 2, "Ei");
	return 0;
}
static int tt_entity_object_step(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_step", false, 2, "Ei");
	return 0;
}
static int tt_entity_object_fly(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_fly", false, 2, "Ei");
	return 0;
}
static int tt_entity_object_say(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_say", false, 2, "Es");
	return 0;
}
static int tt_entity_object_command(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_cmd", false, 2, "Es");
	return 0;
}
static int tt_entity_object_tell(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_tell", false, 3, "EIs");
	return 0;
}
static int tt_entity_object_typing(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_typing", false, 2, "Eb");
	return 0;
}
static int tt_entity_object_set(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_set", false, 2, "Et");
	return 0;
}
static int tt_entity_object_set_mini_tilemap(lua_State* L) {
	return 0;
}
static int tt_entity_object_clone(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_clone", true, 2, "Et");
	return 0;
}
static int tt_entity_object_delete(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_delete", false, 1, "E");
	return 0;
}
static int tt_entity_object_set_callback(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (!thread)
		return 0;
	// Check if the ID is the entity's self
	lua_getfield(L, 1, "_id");
	int n = lua_tointeger(L, -1);
	lua_pop(L, 1);
	if (n != thread->script->entity_id) {
		fprintf(stderr, "Setting callback on non-self entity not supported yet\n");
		return 0;
	}

	// Now to set the function as a callback
	lua_c_function_parameter_check(L, 3, "EFs");
	CallbackTypeID callback_type = get_callback_id_from_name(luaL_checkstring(L, 3), callback_names_self);
	if (callback_type == CALLBACK_INVALID)
		return 0;
	if (thread->script->callback_ref[callback_type] != LUA_NOREF) {
		lua_unref(L, thread->script->callback_ref[callback_type]);
		thread->script->callback_ref[callback_type] = LUA_NOREF;
	}
	thread->send_message(VM_MESSAGE_SET_CALLBACK, callback_type, !lua_isnil(L, 2), nullptr, 0);
	if (!lua_isnil(L, 2)) {
		thread->script->callback_ref[callback_type] = lua_ref(L, 2);
	}
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
		{"tile_lookup",     tt_map_tile_lookup},
		{"map_info",        tt_map_map_info},
		{"within_map",      tt_map_within_map},
		{"set_callback",    tt_map_set_callback},
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
		{"ownersay",        tt_tt_owner_say},
		{"from_json",       tt_tt_decode_json},
		//{"to_json",         tt_tt_encode_json},
		{"memory_used",     tt_tt_memory_used},
		{"memory_free",     tt_tt_memory_free},
		{"set_callback",    tt_tt_set_callback},
		{"_result",         tt_tt_get_result},
        {NULL, NULL},
    };

    static const luaL_Reg entity_object_funcs[] = {
		{"who",             tt_entity_object_who},
		{"move",            tt_entity_object_move},
		{"turn",            tt_entity_object_turn},
		{"step",            tt_entity_object_step},
		{"fly",             tt_entity_object_fly},
		{"say",             tt_entity_object_say},
		{"command",         tt_entity_object_command},
		{"tell",            tt_entity_object_tell},
		{"typing",          tt_entity_object_typing},
		{"set",             tt_entity_object_set},
		{"clone",           tt_entity_object_clone},
		{"delete",          tt_entity_object_delete},
		{"set_callback",    tt_entity_object_set_callback},
		{"set_mini_tilemap", tt_entity_object_set_mini_tilemap},
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

    static const luaL_Reg override_funcs[] = {
		{"print",           tt_custom_print},
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
    luaL_register(L, "_G",            override_funcs);

    luaL_register(L, "Entity",        entity_object_funcs);
    luaL_register(L, "MiniTilemap",   mini_tilemap_object_funcs);
    luaL_register(L, "Bitmap2x4",     bitmap_2x4_object_funcs);

    lua_pop(L, 1);
}
