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
	} else {
		push_json_value(L, root);
	}
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
	{"bot_message_button", CALLBACK_SELF_BOT_COMMAND_BUTTON},
	{"request_received",   CALLBACK_SELF_REQUEST_RECEIVED},
	{"use",                CALLBACK_SELF_USE},
	{"switch_map",         CALLBACK_SELF_SWITCH_MAP},
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

	luaL_getmetatable(L, "Entity");
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
static int tt_entity_here(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_here", true, 0, "");
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
static int tt_entity_owner(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread) {
		char id[20];
		if(thread->script->vm->user_id >= 0) {
			sprintf(id, "%d", thread->script->vm->user_id);
		} else {
			sprintf(id, "~%d", -thread->script->vm->user_id);
		}
		create_entity_table(L, id, thread->script->vm->user_id);
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
static int tt_map_size(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "m_size", true, 0, "");
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
		return thread->send_api_call(L, "s_reset", true, 0, "s");
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
		return thread->send_api_call(L, "s_save", true, -2, "s$");
	return 0;
}
static int tt_storage_list(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "s_list", true, 0, "s");
	return 0;
}
static int tt_mini_tilemap_tile(lua_State* L) {
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	lua_pushinteger(L, (x & 63) | ((y&63) * 64));
	return 1;
}
static int tt_mini_tilemap_new(lua_State* L) {
	lua_newtable(L);

	luaL_getmetatable(L, "MiniTilemap");
	lua_setmetatable(L, -2);

	// Set up the fields
	lua_pushliteral(L, "mini_town.png");
	lua_setfield(L, -2, "tileset_url");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "clickable");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "visible");
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "transparent_tile");

	lua_pushinteger(L, 4);
	lua_setfield(L, -2, "map_width");
	lua_pushinteger(L, 4);
	lua_setfield(L, -2, "map_height");

	lua_pushinteger(L, 8);
	lua_setfield(L, -2, "tile_width");
	lua_pushinteger(L, 8);
	lua_setfield(L, -2, "tile_height");

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "offset_x");
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "offset_y");

	struct mini_tilemap *map = static_cast<struct mini_tilemap *>(lua_newuserdatatagged(L, sizeof(struct mini_tilemap), USER_DATA_MINI_TILEMAP));
	memset(map, 0, sizeof(struct mini_tilemap));
	lua_setfield(L, -2, "_map");

	return 1;
}
static int tt_bitmap_4x2_new(lua_State* L) {
	lua_newtable(L);

	luaL_getmetatable(L, "Bitmap4x2");
	lua_setmetatable(L, -2);

	// Set up the fields
	lua_pushliteral(L, "bitmap.png");
	lua_setfield(L, -2, "tileset_url");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "clickable");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "visible");
	lua_pushinteger(L, -1);
	lua_setfield(L, -2, "transparent_tile");

	lua_pushinteger(L, 8);
	lua_setfield(L, -2, "map_width");
	lua_pushinteger(L, 16);
	lua_setfield(L, -2, "map_height");

	lua_pushinteger(L, 4);
	lua_setfield(L, -2, "tile_width");
	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "tile_height");

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "offset_x");
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "offset_y");

	// Color you're currently drawing with
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "color");
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "invert");

	struct mini_tilemap *map = static_cast<struct mini_tilemap *>(lua_newuserdatatagged(L, sizeof(struct mini_tilemap), USER_DATA_MINI_TILEMAP));
	memset(map, 0, sizeof(struct mini_tilemap));
	lua_setfield(L, -2, "_map");

	return 1;
}
static int tt_bitmap_sprite_new(lua_State* L) {
	struct bitmap_sprite *sprite = static_cast<struct bitmap_sprite *>(lua_newuserdatatagged(L, sizeof(struct bitmap_sprite), USER_DATA_BITMAP_SPRITE));
	memset(sprite, 0, sizeof(struct bitmap_sprite));

	size_t len = lua_objlen(L, 1);
	for (size_t i = 1; i <= len; i++) {
		lua_rawgeti(L, 1, i);
		unsigned int row = lua_tounsigned(L, -1);
		lua_pop(L, 1);
		sprite->pixels[i-1] = row;

		if (row)
			sprite->height = i;
		int width = 0;
		while (row) {
			row >>= 1;
			width++;
		}
		if (width > sprite->width)
			sprite->width = width;
	}
	return 1;
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
		return thread->send_api_call(L, "ownersay", false, 1, "$");
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

static int tt_tt_run_text_item(lua_State *L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "runitem", true, 1, "I");
	return 0;
}
static int tt_tt_read_text_item(lua_State *L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "readitem", true, 1, "I");
	return 0;
}
static int tt_tt_stop_script(lua_State *L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		thread->send_api_call(L, "stopscript", false, 0, "");
	return lua_break(L);
}
static int tt_tt_start_thread(lua_State *L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		thread->script->start_thread(L);
	return 0;
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
static int tt_entity_object_xy(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_xy", true, 1, "E");
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
		return thread->send_api_call(L, "e_say", false, 2, "E$");
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
		return thread->send_api_call(L, "e_tell", false, 3, "EI$");
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
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_minitilemap", false, 2, "EM");
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
	lua_c_function_parameter_check(L, 3, "EFs");
	lua_getfield(L, 1, "_id");
	int n = lua_tointeger(L, -1);
	lua_pop(L, 1);
	if (n != thread->script->entity_id) {
		fprintf(stderr, "Setting callback on non-self entity not supported yet\n");
		return 0;
	}

	// Now to set the function as a callback
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
static int tt_entity_object_is_loaded(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_isloaded", true, 1, "E");
	return 0;
}
static int tt_entity_object_have_permission(lua_State* L) {
	ScriptThread *thread = static_cast<ScriptThread*>(lua_getthreaddata(L));
	if (thread)
		return thread->send_api_call(L, "e_havepermission", true, 1, "Es");
	return 0;
}

/////////////////////////////////////////////////
// Mini tilemap functions
/////////////////////////////////////////////////

static int tt_mini_tilemap_object_put(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int t = luaL_checkinteger(L, 4);
	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		map->tilemap[y][x] = t;
	}
	return 0;
}
static int tt_mini_tilemap_object_get(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		lua_pushinteger(L, map->tilemap[y][x]);
		return 1;
	}
	return 0;
}
static int tt_mini_tilemap_object_clear(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int t = luaL_checkinteger(L, 2);
	for (int x=0; x<MINI_TILEMAP_MAX_MAP_WIDTH; x++) {
		for (int y=0; y<MINI_TILEMAP_MAX_MAP_HEIGHT; y++) {
			map->tilemap[y][x] = t;
		}
	}
	return 0;
}
static int tt_mini_tilemap_object_rectfill(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int w = luaL_checkinteger(L, 4);
	int h = luaL_checkinteger(L, 5);
	int t = luaL_checkinteger(L, 6);

	int x1 = (x >= 0) ? x : 0;
	int y1 = (y >= 0) ? y : 0;
	int x2 = ((x + w - 1) < MINI_TILEMAP_MAX_MAP_WIDTH)  ? (x + w - 1) : (MINI_TILEMAP_MAX_MAP_WIDTH-1);
	int y2 = ((y + h - 1) < MINI_TILEMAP_MAX_MAP_HEIGHT) ? (y + h - 1) : (MINI_TILEMAP_MAX_MAP_HEIGHT-1);

	for (int yi = y1; yi <= y2; yi++) {
		for (int xi = x1; xi <= x2; xi++) {
			map->tilemap[yi][xi] = t;
		}
	}
	return 0;
}

static int wrap_positive(int x, int max) {
	return ((x % max) + max) % max;
}
static int tt_mini_tilemap_object_paste(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map_dst = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map_dst)
		return 0;
	lua_pop(L, 1);

	lua_getfield(L, 2, "_map");
	struct mini_tilemap *map_src = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map_src)
		return 0;
	lua_pop(L, 1);

	lua_getfield(L, 2, "transparent_tile");
	int transparent_tile = lua_tointeger(L, -1);
	lua_pop(L, 1);

	int x_dst = luaL_optunsigned(L, 3, 0);
	int y_dst = luaL_optunsigned(L, 4, 0);
	int x_src = luaL_optunsigned(L, 5, 0);
	int y_src = luaL_optunsigned(L, 6, 0);
	int w     = luaL_optunsigned(L, 7, MINI_TILEMAP_MAX_MAP_WIDTH);
	int h     = luaL_optunsigned(L, 8, MINI_TILEMAP_MAX_MAP_HEIGHT);
	bool xflip = luaL_optboolean(L, 9,  false);
	bool yflip = luaL_optboolean(L, 10, false);
	bool overwrite = luaL_optboolean(L, 11, false);
	if (w <= 0 || h <= 0)
		return 0;
	if (w > MINI_TILEMAP_MAX_MAP_WIDTH)
		w = MINI_TILEMAP_MAX_MAP_WIDTH;
	if (h > MINI_TILEMAP_MAX_MAP_HEIGHT)
		h = MINI_TILEMAP_MAX_MAP_HEIGHT;

	for (int yd = y_dst; yd < (y_dst + w); yd++) {
		for (int xd = x_dst; xd < (x_dst + w); xd++) {
			if (xd < 0 || yd < 0 || xd >= MINI_TILEMAP_MAX_MAP_WIDTH || yd >= MINI_TILEMAP_MAX_MAP_HEIGHT)
				continue;
			int xs = ((xflip) ? (w - (xd-x_dst) - 1) : (xd-x_dst)) + x_src;
			int ys = ((yflip) ? (h - (yd-y_dst) - 1) : (yd-y_dst)) + y_src;
			if (xs < 0 || ys < 0 || xs >= MINI_TILEMAP_MAX_MAP_WIDTH || ys >= MINI_TILEMAP_MAX_MAP_HEIGHT)
				continue;
			if ((map_src->tilemap[ys][xs] != transparent_tile) || overwrite)
				map_dst->tilemap[yd][xd] = map_src->tilemap[ys][xs];
		}
	}
	return 0;
}

static int tt_mini_tilemap_object_scroll(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x  = luaL_checkinteger(L, 2);
	int y  = luaL_checkinteger(L, 3);
	int w  = luaL_checkinteger(L, 4);
	int h  = luaL_checkinteger(L, 5);
	int sx = luaL_checkinteger(L, 6);
	int sy = luaL_checkinteger(L, 7);
	if (w <= 0 || h <= 0)
		return 0;
	if (sx == 0 && sy == 0)
		return 0;

	int x1 = (x >= 0) ? x : 0;
	int y1 = (y >= 0) ? y : 0;
	int x2 = ((x + w - 1) < MINI_TILEMAP_MAX_MAP_WIDTH)  ? (x + w - 1) : (MINI_TILEMAP_MAX_MAP_WIDTH-1);
	int y2 = ((y + h - 1) < MINI_TILEMAP_MAX_MAP_HEIGHT) ? (y + h - 1) : (MINI_TILEMAP_MAX_MAP_HEIGHT-1);

	struct mini_tilemap copy = *map;
	for (int yi = y1; yi <= y2; yi++) {
		for (int xi = x1; xi <= x2; xi++) {
			map->tilemap[yi][xi] = copy.tilemap[wrap_positive(yi-y1+sy,h)+y1][wrap_positive(xi-x1+sx,w)+x1];
		}
	}
	return 0;
}

/////////////////////////////////////////////////
// Bitmap 4x2 functions
/////////////////////////////////////////////////

#define BITMAP_COLOR_MASK 0b110000110000
#define BITMAP_PIXEL_MASK 0b001111001111
static int get_bitmap_color_bits(lua_State* L) {
	lua_getfield(L, 1, "color");
	int color = luaL_checkinteger(L, -1) & 15;
	lua_pop(L, 1);
	return ((color & 0x3) << 4) | ((color & 0xC) << 6);
}
static int get_draw_bit_with_invert(lua_State* L, int arg) {
	lua_getfield(L, 1, "invert");
	bool v = lua_toboolean(L, -1) ^ lua_toboolean(L, arg);
	lua_pop(L, 1);
	return v;
}
void put_bitmap_pixel(struct mini_tilemap *map, int pixel_x, int pixel_y, int color_bits, bool value) {
	int x = pixel_x / 4;
	int y = pixel_y / 2;

	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		if (value)
			map->tilemap[y][x] |= 1 << ( (pixel_x&3) + ((pixel_y&1)*6) );
		else
			map->tilemap[y][x] &= ~(1 << ( (pixel_x&3) + ((pixel_y&1)*6) ));
		map->tilemap[y][x] = (map->tilemap[y][x] & BITMAP_PIXEL_MASK) | color_bits;
	}
}
bool get_bitmap_pixel(struct mini_tilemap *map, int pixel_x, int pixel_y) {
	int x = pixel_x / 4;
	int y = pixel_y / 2;

	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		return (map->tilemap[y][x] & (1 << ( (pixel_x&3) + ((pixel_y&1)*6) ))) != 0;
	}
	return false;
}
void toggle_bitmap_pixel(struct mini_tilemap *map, int pixel_x, int pixel_y, int color_bits) {
	int x = pixel_x / 4;
	int y = pixel_y / 2;

	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		map->tilemap[y][x] ^= 1 << ( (pixel_x&3) + ((pixel_y&1)*6) );
		map->tilemap[y][x] = (map->tilemap[y][x] & BITMAP_PIXEL_MASK) | color_bits;
	}
}
void rect_fill_bitmap(struct mini_tilemap *map, int x, int y, int w, int h, int color_bits, bool value) {
	if ((x >= BITMAP_MAX_MAP_WIDTH) || ((x + w) < 0) || (y >= BITMAP_MAX_MAP_HEIGHT) || ((y + h) < 0))
		return;
	int x1 = (x >= 0) ? x : 0;
	int y1 = (y >= 0) ? y : 0;
	int x2 = ((x + w - 1) < BITMAP_MAX_MAP_WIDTH)  ? (x + w - 1) : (BITMAP_MAX_MAP_WIDTH-1);
	int y2 = ((y + h - 1) < BITMAP_MAX_MAP_HEIGHT) ? (y + h - 1) : (BITMAP_MAX_MAP_HEIGHT-1);

	for (int yi = y1; yi <= y2; yi++) {
		for (int xi = x1; xi <= x2; xi++) {
			put_bitmap_pixel(map, xi, yi, color_bits, value);
		}
	}
}
void rect_fill_bitmap_xor(struct mini_tilemap *map, int x, int y, int w, int h, int color_bits) {
	if ((x >= BITMAP_MAX_MAP_WIDTH) || ((x + w) < 0) || (y >= BITMAP_MAX_MAP_HEIGHT) || ((y + h) < 0))
		return;
	int x1 = (x >= 0) ? x : 0;
	int y1 = (y >= 0) ? y : 0;
	int x2 = ((x + w - 1) < BITMAP_MAX_MAP_WIDTH)  ? (x + w - 1) : (BITMAP_MAX_MAP_WIDTH-1);
	int y2 = ((y + h - 1) < BITMAP_MAX_MAP_HEIGHT) ? (y + h - 1) : (BITMAP_MAX_MAP_HEIGHT-1);

	for (int yi = y1; yi <= y2; yi++) {
		for (int xi = x1; xi <= x2; xi++) {
			toggle_bitmap_pixel(map, xi, yi, color_bits);
		}
	}
}

static int tt_bitmap_4x2_object_put(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int pixel_x = luaL_checkinteger(L, 2);
	int pixel_y = luaL_checkinteger(L, 3);
	put_bitmap_pixel(map, pixel_x, pixel_y, get_bitmap_color_bits(L), get_draw_bit_with_invert(L, 4));
	return 0;
}
static int tt_bitmap_4x2_object_xor(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int pixel_x = luaL_checkinteger(L, 2);
	int pixel_y = luaL_checkinteger(L, 3);
	toggle_bitmap_pixel(map, pixel_x, pixel_y, get_bitmap_color_bits(L));
	return 0;
}
static int tt_bitmap_4x2_object_get(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int pixel_x = luaL_checkinteger(L, 2);
	int pixel_y = luaL_checkinteger(L, 3);
	int x = pixel_x / 4;
	int y = pixel_y / 2;

	if (x >= 0 && y >= 0 && x < MINI_TILEMAP_MAX_MAP_WIDTH && y < MINI_TILEMAP_MAX_MAP_HEIGHT) {
		lua_getfield(L, 1, "invert");
		bool invert = lua_toboolean(L, -1);
		lua_pop(L, 1);
		lua_pushboolean(L, invert ^ (0 != (map->tilemap[y][x] & (1 << ( (pixel_x&3) + ((pixel_y&1)*6) )))) );
	} else {
		lua_pushboolean(L, false );
	}
	return 1;
}
static int tt_bitmap_4x2_object_clear(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	lua_getfield(L, 1, "invert");
	bool invert = lua_toboolean(L, -1);
	lua_pop(L, 1);
	uint16_t clear_to = get_bitmap_color_bits(L) | ((lua_toboolean(L, 2) ^ invert) ? BITMAP_PIXEL_MASK : 0);
	for (int x=0; x<MINI_TILEMAP_MAX_MAP_WIDTH; x++)
		for (int y=0; y<MINI_TILEMAP_MAX_MAP_HEIGHT; y++)
			map->tilemap[y][x] = clear_to;
	return 0;
}
static int tt_bitmap_4x2_object_rectfill(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int w = luaL_checkinteger(L, 4);
	int h = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	bool value = get_draw_bit_with_invert(L, 6);
	rect_fill_bitmap(map, x, y, w, h, color_bits, value);
	return 0;
}
static int tt_bitmap_4x2_object_rectfill_xor(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int w = luaL_checkinteger(L, 4);
	int h = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	rect_fill_bitmap_xor(map, x, y, w, h, color_bits);
	return 0;
}
static int tt_bitmap_4x2_object_rect(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int w = luaL_checkinteger(L, 4);
	int h = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	bool value = get_draw_bit_with_invert(L, 6);
	rect_fill_bitmap(map, x,     y,     1,   h,   color_bits, value);
	rect_fill_bitmap(map, x+w-1, y,     1,   h,   color_bits, value);
	rect_fill_bitmap(map, x+1,   y,     w-2, 1,   color_bits, value);
	rect_fill_bitmap(map, x+1,   y+h-1, w-2, 1,   color_bits, value);
	return 0;
}
static int tt_bitmap_4x2_object_rect_xor(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	int w = luaL_checkinteger(L, 4);
	int h = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	rect_fill_bitmap_xor(map, x,     y,     1,   h,   color_bits);
	rect_fill_bitmap_xor(map, x+w-1, y,     1,   h,   color_bits);
	rect_fill_bitmap_xor(map, x+1,   y,     w-2, 1,   color_bits);
	rect_fill_bitmap_xor(map, x+1,   y+h-1, w-2, 1,   color_bits);
	return 0;
}

void bitmap_draw_line(struct mini_tilemap *map, int x1, int y1, int x2, int y2, int color_bits, int value) {
	if (x1 < -BITMAP_MAX_MAP_WIDTH || x1 > BITMAP_MAX_MAP_WIDTH*2 || y1 < -BITMAP_MAX_MAP_HEIGHT || y1 > BITMAP_MAX_MAP_HEIGHT*2
	  || x2 < -BITMAP_MAX_MAP_WIDTH || x2 > BITMAP_MAX_MAP_WIDTH*2 || y2 < -BITMAP_MAX_MAP_HEIGHT || y2 > BITMAP_MAX_MAP_HEIGHT*2)
		return;
	// Brensenham's line algorithm, from Wikipedia										
	int dx = abs(x2 - x1);
	int sx = x1 < x2 ? 1 : -1;
	int dy = -abs(y2 - y1);
	int sy = y1 < y2 ? 1 : -1;
	int error = dx + dy;
	int x = x1;
	int y = y1;

	while(1) {
		if (value >= 0)
			put_bitmap_pixel(map, x, y, color_bits, value);
		else
			toggle_bitmap_pixel(map, x, y, color_bits);
		if(x == x2 && y == y2)
			break;
		int e2 = 2 * error;
		if(e2 >= dy) {
			if(x == x2)
				break;
			error = error + dy;
			x += sx;
		}
		if(e2 <= dx) {
			if(y == y2)
				break;
			error = error + dx;
			y += sy;
		}
	}
}
static int tt_bitmap_4x2_object_line(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x1 = luaL_checkinteger(L, 2);
	int y1 = luaL_checkinteger(L, 3);
	int x2 = luaL_checkinteger(L, 4);
	int y2 = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	bool value = get_draw_bit_with_invert(L, 6);
	bitmap_draw_line(map, x1, y1, x2, y2, color_bits, value);
	return 0;
}
static int tt_bitmap_4x2_object_line_xor(lua_State* L) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return 0;
	int x1 = luaL_checkinteger(L, 2);
	int y1 = luaL_checkinteger(L, 3);
	int x2 = luaL_checkinteger(L, 4);
	int y2 = luaL_checkinteger(L, 5);
	int color_bits = get_bitmap_color_bits(L);
	bool value = get_draw_bit_with_invert(L, 6);
	bitmap_draw_line(map, x1, y1, x2, y2, color_bits, value);
	return 0;
}

// https://robey.lag.net/2010/01/23/tiny-monospace-font.html
const uint8_t font_4x6[] = {0, 0, 0, 0, 0, 0, 64, 64, 64, 0, 64, 0, 160, 160, 0, 0, 0, 0, 160, 224, 160, 224, 160, 0, 96, 192, 96, 192, 64, 0, 128, 32, 64, 128, 32, 0, 192, 192, 224, 160, 96, 0, 64, 64, 0, 0, 0, 0, 32, 64, 64, 64, 32, 0, 128, 64, 64, 64, 128, 0, 160, 64, 160, 0, 0, 0, 0, 64, 224, 64, 0, 0, 0, 0, 0, 64, 128, 0, 0, 0, 224, 0, 0, 0, 0, 0, 0, 0, 64, 0, 32, 32, 64, 128, 128, 0, 96, 160, 160, 160, 192, 0, 64, 192, 64, 64, 64, 0, 192, 32, 64, 128, 224, 0, 192, 32, 64, 32, 192, 0, 160, 160, 224, 32, 32, 0, 224, 128, 192, 32, 192, 0, 96, 128, 224, 160, 224, 0, 224, 32, 64, 128, 128, 0, 224, 160, 224, 160, 224, 0, 224, 160, 224, 32, 192, 0, 0, 64, 0, 64, 0, 0, 0, 64, 0, 64, 128, 0, 32, 64, 128, 64, 32, 0, 0, 224, 0, 224, 0, 0, 128, 64, 32, 64, 128, 0, 224, 32, 64, 0, 64, 0, 64, 160, 224, 128, 96, 0, 64, 160, 224, 160, 160, 0, 192, 160, 192, 160, 192, 0, 96, 128, 128, 128, 96, 0, 192, 160, 160, 160, 192, 0, 224, 128, 224, 128, 224, 0, 224, 128, 224, 128, 128, 0, 96, 128, 224, 160, 96, 0, 160, 160, 224, 160, 160, 0, 224, 64, 64, 64, 224, 0, 32, 32, 32, 160, 64, 0, 160, 160, 192, 160, 160, 0, 128, 128, 128, 128, 224, 0, 160, 224, 224, 160, 160, 0, 160, 224, 224, 224, 160, 0, 64, 160, 160, 160, 64, 0, 192, 160, 192, 128, 128, 0, 64, 160, 160, 224, 96, 0, 192, 160, 224, 192, 160, 0, 96, 128, 64, 32, 192, 0, 224, 64, 64, 64, 64, 0, 160, 160, 160, 160, 96, 0, 160, 160, 160, 64, 64, 0, 160, 160, 224, 224, 160, 0, 160, 160, 64, 160, 160, 0, 160, 160, 64, 64, 64, 0, 224, 32, 64, 128, 224, 0, 224, 128, 128, 128, 224, 0, 0, 128, 64, 32, 0, 0, 224, 32, 32, 32, 224, 0, 64, 160, 0, 0, 0, 0, 0, 0, 0, 0, 224, 0, 128, 64, 0, 0, 0, 0, 0, 192, 96, 160, 224, 0, 128, 192, 160, 160, 192, 0, 0, 96, 128, 128, 96, 0, 32, 96, 160, 160, 96, 0, 0, 96, 160, 192, 96, 0, 32, 64, 224, 64, 64, 0, 0, 96, 160, 224, 32, 64, 128, 192, 160, 160, 160, 0, 64, 0, 64, 64, 64, 0, 32, 0, 32, 32, 160, 64, 128, 160, 192, 192, 160, 0, 192, 64, 64, 64, 224, 0, 0, 224, 224, 224, 160, 0, 0, 192, 160, 160, 160, 0, 0, 64, 160, 160, 64, 0, 0, 192, 160, 160, 192, 128, 0, 96, 160, 160, 96, 32, 0, 96, 128, 128, 128, 0, 0, 96, 192, 96, 192, 0, 64, 224, 64, 64, 96, 0, 0, 160, 160, 160, 96, 0, 0, 160, 160, 224, 64, 0, 0, 160, 224, 224, 224, 0, 0, 160, 64, 64, 160, 0, 0, 160, 160, 96, 32, 64, 0, 224, 96, 192, 224, 0, 96, 64, 128, 64, 96, 0, 64, 64, 0, 64, 64, 0, 192, 64, 32, 64, 192, 0, 96, 192, 0, 0, 0, 0, 224, 224, 224, 224, 224, 0};
static void bitmap_write_text(lua_State *L, bool toggle) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return;
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	const char *text;
	bool value;
	if (toggle) {
		text = luaL_checkstring(L, 4);
	} else {
		text = luaL_checkstring(L, 5);
		value = get_draw_bit_with_invert(L, 4);
	}
	int color_bits = get_bitmap_color_bits(L);

	int chars_so_far = 0;
	for (const char *c = text; *c; c++) {
		char ch = *c - 0x20;
		if (ch < 0 || ch > (0x7F - 0x20))
			ch = '?' - 0x20;
		for (int yc = 0; yc < 6; yc++) {
			unsigned char line = font_4x6[ch*6+yc];
			for (int xc = 0; xc < 4; xc++) {
				if (line & (0x80 >> xc)) {
					if (toggle)
						put_bitmap_pixel(map, x+xc, y+yc, color_bits, value);
					else
						toggle_bitmap_pixel(map, x+xc, y+yc, color_bits);
				}
			}
		}
		x += 4;
		chars_so_far++;
		if (chars_so_far >= 20)
			return;
	}
}
static int tt_bitmap_4x2_object_text(lua_State* L) {
	bitmap_write_text(L, false);
	return 0;
}
static int tt_bitmap_4x2_object_text_xor(lua_State* L) {
	bitmap_write_text(L, false);
	return 0;
}

static void bitmap_draw_sprite(lua_State* L, int value) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map)
		return;
	struct bitmap_sprite *sprite = static_cast<struct bitmap_sprite*>(lua_touserdatatagged(L, 2, USER_DATA_BITMAP_SPRITE));
	if (!sprite)
		return;
	int x = luaL_checkinteger(L, 3);
	int y = luaL_checkinteger(L, 4);
	bool xflip = luaL_optboolean(L, (value == -1) ? 5 : 6, false);
	bool yflip = luaL_optboolean(L, (value == -1) ? 6 : 7, false);
	int color_bits = get_bitmap_color_bits(L);

	for (int ys = 0; ys < sprite->height; ys++) {
		unsigned int line = sprite->pixels[yflip ? (sprite->height - ys - 1) : ys];
		for (int xs = 0; xs < sprite->width; xs++) {
			if (line & 1) {
				int xd = (xflip ? (sprite->width - xs - 1) : xs) + x;
				if (value == -1)
					toggle_bitmap_pixel(map, xd, y + ys, color_bits);
				else
					put_bitmap_pixel(map, xd, y + ys, color_bits, value);
			}
			line >>= 1;
		}
	}
}
static int tt_bitmap_4x2_object_draw_sprite(lua_State* L) {
	bitmap_draw_sprite(L, get_draw_bit_with_invert(L, 5));
	return 0;
}
static int tt_bitmap_4x2_object_draw_sprite_xor(lua_State* L) {
	bitmap_draw_sprite(L, -1);
	return 0;
}

static void bitmap_paste(lua_State *L, bool toggle) {
	lua_getfield(L, 1, "_map");
	struct mini_tilemap *map_dst = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map_dst)
		return;
	lua_pop(L, 1);

	lua_getfield(L, 2, "_map");
	struct mini_tilemap *map_src = static_cast<struct mini_tilemap*>(lua_touserdatatagged(L, -1, USER_DATA_MINI_TILEMAP));
	if (!map_src)
		return;
	lua_pop(L, 1);

	lua_getfield(L, 1, "invert");
	bool invert_dst = lua_toboolean(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 1, "invert");
	bool invert_src = lua_toboolean(L, -1);
	lua_pop(L, 1);

	int x_dst = luaL_optunsigned(L, 3, 0);
	int y_dst = luaL_optunsigned(L, 4, 0);
	int x_src = luaL_optunsigned(L, 5, 0);
	int y_src = luaL_optunsigned(L, 6, 0);
	int w     = luaL_optunsigned(L, 7, BITMAP_MAX_MAP_WIDTH);
	int h     = luaL_optunsigned(L, 8, BITMAP_MAX_MAP_HEIGHT);
	bool xflip = luaL_optboolean(L, 9,  false);
	bool yflip = luaL_optboolean(L, 10, false);
	int color_bits = get_bitmap_color_bits(L);

	if (w <= 0 || h <= 0)
		return;
	if (w > BITMAP_MAX_MAP_WIDTH)
		w = BITMAP_MAX_MAP_WIDTH;
	if (h > BITMAP_MAX_MAP_HEIGHT)
		h = BITMAP_MAX_MAP_HEIGHT;

	for (int yd = y_dst; yd < (y_dst + w); yd++) {
		for (int xd = x_dst; xd < (x_dst + w); xd++) {
			if (xd < 0 || yd < 0 || xd >= BITMAP_MAX_MAP_WIDTH || yd >= BITMAP_MAX_MAP_HEIGHT)
				continue;
			int xs = ((xflip) ? (w - (xd-x_dst) - 1) : (xd-x_dst)) + x_src;
			int ys = ((yflip) ? (h - (yd-y_dst) - 1) : (yd-y_dst)) + y_src;
			if (xs < 0 || ys < 0 || xs >= BITMAP_MAX_MAP_WIDTH || ys >= BITMAP_MAX_MAP_HEIGHT)
				continue;
			if (get_bitmap_pixel(map_src, xs, ys) ^ invert_src) {
				if (toggle)
					toggle_bitmap_pixel(map_dst, xd, yd, color_bits);
				else
					put_bitmap_pixel(map_dst, xd, yd, color_bits, !invert_dst);
			}
		}
	}
}
static int tt_bitmap_4x2_object_paste(lua_State* L) {
	bitmap_paste(L, false);
	return 0;
}
static int tt_bitmap_4x2_object_paste_xor(lua_State* L) {
	bitmap_paste(L, true);
	return 0;
}

// --------------------------------------------------------

void register_lua_api(lua_State* L) {
    static const luaL_Reg entity_funcs[] = {
		{"new",   tt_entity_new},
		{"here",  tt_entity_here},
		{"me",    tt_entity_me},
		{"owner", tt_entity_owner},
		{"get",   tt_entity_get},
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
		{"size",            tt_map_size},
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

    static const luaL_Reg bitmap_4x2_funcs[] = {
		{"new",        tt_bitmap_4x2_new},
        {NULL, NULL},
    };

    static const luaL_Reg bitmap_sprite_funcs[] = {
		{"new",        tt_bitmap_sprite_new},
        {NULL, NULL},
    };

    static const luaL_Reg misc_funcs[] = {
		{"sleep",           tt_tt_sleep},
		{"owner_say",       tt_tt_owner_say},
		{"from_json",       tt_tt_decode_json},
		//{"to_json",         tt_tt_encode_json},
		{"memory_used",     tt_tt_memory_used},
		{"memory_free",     tt_tt_memory_free},
		{"set_callback",    tt_tt_set_callback},
		{"_result",         tt_tt_get_result},
		{"run_text_item",   tt_tt_run_text_item},
		{"read_text_item",  tt_tt_read_text_item},
		{"stop_script",     tt_tt_stop_script},
		{"start_thread",    tt_tt_start_thread},
        {NULL, NULL},
    };

    static const luaL_Reg entity_object_funcs[] = {
		{"who",             tt_entity_object_who},
		{"xy",              tt_entity_object_xy},
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
		{"is_loaded",       tt_entity_object_is_loaded},
		{"have_permission", tt_entity_object_have_permission},
		{"set_mini_tilemap", tt_entity_object_set_mini_tilemap},
        {NULL, NULL},
    };

    static const luaL_Reg mini_tilemap_object_funcs[] = {
		{"put",        tt_mini_tilemap_object_put},
		{"get",        tt_mini_tilemap_object_get},
		{"clear",      tt_mini_tilemap_object_clear},
		{"rect_fill",  tt_mini_tilemap_object_rectfill},
		{"paste",      tt_mini_tilemap_object_paste},
		{"scroll",     tt_mini_tilemap_object_scroll},
        {NULL, NULL},
    };

    static const luaL_Reg bitmap_4x2_object_funcs[] = {
		{"put",             tt_bitmap_4x2_object_put},
		{"xor",             tt_bitmap_4x2_object_xor},
		{"get",             tt_bitmap_4x2_object_get},
		{"clear",           tt_bitmap_4x2_object_clear},
		{"rect_fill",       tt_bitmap_4x2_object_rectfill},
		{"rect_fill_xor",   tt_bitmap_4x2_object_rectfill_xor},
		{"rect",            tt_bitmap_4x2_object_rect},
		{"rect_xor",        tt_bitmap_4x2_object_rect_xor},
		{"line",            tt_bitmap_4x2_object_line},
		{"line_xor",        tt_bitmap_4x2_object_line_xor},
		{"paste",           tt_bitmap_4x2_object_paste},
		{"paste_xor",       tt_bitmap_4x2_object_paste_xor},
		//{"scroll",          tt_bitmap_4x2_object_scroll},
		{"text",            tt_bitmap_4x2_object_text},
		{"text_xor",        tt_bitmap_4x2_object_text_xor},
		{"draw_sprite",     tt_bitmap_4x2_object_draw_sprite},
		{"draw_sprite_xor", tt_bitmap_4x2_object_draw_sprite_xor},
        {NULL, NULL},
    };

    static const luaL_Reg override_funcs[] = {
		{"print",           tt_custom_print},
        {NULL, NULL},
    };

    luaL_register(L, "entity",        entity_funcs);
    luaL_register(L, "map",           map_funcs);
    luaL_register(L, "storage",       storage_funcs);
    luaL_register(L, "mini_tilemap",  mini_tilemap_funcs);
    luaL_register(L, "bitmap_4x2",    bitmap_4x2_funcs);
    luaL_register(L, "bitmap_sprite", bitmap_sprite_funcs);
    luaL_register(L, "tt",            misc_funcs);
    luaL_register(L, "_G",            override_funcs);
    lua_pop(L, 8);

    luaL_register(L, "Entity",        entity_object_funcs);
    luaL_register(L, "MiniTilemap",   mini_tilemap_object_funcs);
    luaL_register(L, "Bitmap4x2",     bitmap_4x2_object_funcs);
    lua_pop(L, 3);

	// ------------

    luaL_newmetatable(L, "Entity");
	lua_getglobal(L, "Entity");
	lua_setfield(L, -2, "__index");
	lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    luaL_newmetatable(L, "MiniTilemap");
	lua_getglobal(L, "MiniTilemap");
	lua_setfield(L, -2, "__index");
	lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    luaL_newmetatable(L, "Bitmap4x2");
	lua_getglobal(L, "Bitmap4x2");
	lua_setfield(L, -2, "__index");
	lua_setreadonly(L, -1, true);
    lua_pop(L, 1);
}
