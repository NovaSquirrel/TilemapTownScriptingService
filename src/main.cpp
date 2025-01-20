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
#include <unistd.h>
#include <thread>
#include <chrono>

std::mutex outgoing_messages_mtx;
size_t all_vms_bytecode_size;
char *all_vms_bytecode;
std::unordered_map<int, std::unique_ptr<VM>> vm_by_user;
void send_outgoing_message(VM_MessageType type, unsigned int user_id, int entity_id, unsigned int other_id, unsigned char status, void *data, size_t data_len);

///////////////////////////////////////////////////////////

int main(void) {
	// Compile the global script before doing anything else
	const char *script_to_load_into_all_vms = "for k, v in {{\"entity\", \"new\"},{\"map\", \"who\"},{\"map\", \"turf_at\"},{\"map\", \"objs_at\"},{\"map\", \"dense_at\"},{\"map\", \"tile_info\"},{\"map\", \"map_info\"},{\"map\", \"within_map\"},{\"storage\", \"load\"},{\"storage\", \"list\"},{\"Entity\", \"who\"},{\"Entity\", \"clone\"}} do local original = _G[v[1]][v[2]]; _G[v[1]][v[2]] = function(...) original(unpack({...})); return tt._result(); end; end";
	all_vms_bytecode = luau_compile(script_to_load_into_all_vms, strlen(script_to_load_into_all_vms), NULL, &all_vms_bytecode_size);

	VM l = VM(1);
	l.start_thread();

	bool quitting = false;
	while (!quitting) {
		VM_MessageType type = (VM_MessageType)getchar();
		unsigned char length1 = getchar();
		unsigned char length2 = getchar();
		unsigned char length3 = getchar();
		size_t data_length = length1 | (length2<<8) | (length3<<16);
		int user_id, entity_id, other_id;
		if (fread(&user_id,   sizeof(int), 1, stdin) != 1)
			break;
		if (fread(&entity_id, sizeof(int), 1, stdin) != 1)
			break;
		if (fread(&other_id,  sizeof(int), 1, stdin) != 1)
			break;
		unsigned char status = getchar();
		void *data = nullptr;

		if (data_length) {
			data = malloc(data_length);
			if (fread(data, 1, data_length, stdin) != data_length)
				break;
		} 

		switch(type) {
			case VM_MESSAGE_PING:
				send_outgoing_message(VM_MESSAGE_PONG, user_id, entity_id, other_id, status, nullptr, 0);
				break;
			case VM_MESSAGE_PONG:
				fprintf(stderr, "PONG received\n");
				break;
			case VM_MESSAGE_VERSION_CHECK:
				send_outgoing_message(VM_MESSAGE_PONG, 0, 0, 1, 0, nullptr, 0);
				break;
			case VM_MESSAGE_SHUTDOWN:
				fprintf(stderr, "Shutting down\n");
				quitting = true;
				break;
			case VM_MESSAGE_START_SCRIPT:
			case VM_MESSAGE_RUN_CODE:
			case VM_MESSAGE_STOP_SCRIPT:
			case VM_MESSAGE_API_CALL:
			case VM_MESSAGE_API_CALL_GET:
			case VM_MESSAGE_CALLBACK:
			{
				auto it = vm_by_user.find(user_id);
				if(it != vm_by_user.end()) {
					VM *vm = (*it).second.get();
					vm->receive_message(type, entity_id, other_id, status, data, data_length);
				}	
				break;
			}
		}
	}

}
