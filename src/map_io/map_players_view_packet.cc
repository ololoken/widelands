/*
 * Copyright (C) 2007-2025 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "map_io/map_players_view_packet.h"

#include <atomic>
#include <sstream>

#include "base/log.h"
#include "base/string.h"
#include "base/wexception.h"
#include "economy/flag.h"
#include "economy/road.h"
#include "io/fileread.h"
#include "io/filewrite.h"
#include "logic/editor_game_base.h"
#include "logic/field.h"
#include "logic/game_data_error.h"
#include "logic/map_objects/descriptions.h"
#include "logic/map_objects/tribes/tribe_descr.h"
#include "logic/player.h"

namespace Widelands {

/* Changelog:
 * 5: v1.1
 * 6: Removed obsolete data fields time_triangle_last_surveyed and resource_amounts.
 * 7: changes ConstructionsiteInfoPacket
 */
constexpr uint16_t kCurrentPacketVersion = 7;

/// Vision values for saveloading. We only care about PreviouslySeen and Revealed states here,
/// the details about current player objects' vision are reconstructed when loading map object data.
/// The values are stored in savegames so don't change them.
enum class SavedVisionState {
	kNone = '0',            // Neither of the below states
	kPreviouslySeen = 'P',  // Previously seen, unseen now
	kRevealed = 'R',        // Explicitly revealed
	kHidden = 'H'           // Explicitly hidden
};

inline bool from_unsigned(unsigned value) {
	return value == 1;
}

void MapPlayersViewPacket::read(FileSystem& fs, EditorGameBase& egbase) {
	FileRead fr;
	if (!fr.try_open(fs, "binary/view")) {
		// TODO(Nordfriese): Savegame compatibility – require this packet after v1.0
		// and suppress this warning when starting a scenario (where it's a false-positive).
		verb_log_warn(
		   "New-style view packet not found. There may be strange effects regarding unseen "
		   "areas.\n");
		return;
	}

	try {
		uint16_t const packet_version = fr.unsigned_16();
		const Map& map = egbase.map();
		if (packet_version >= 3 && packet_version <= kCurrentPacketVersion) {
			const PlayerNumber nr_players = fr.unsigned_8();
			if (map.get_nrplayers() != nr_players) {
				throw wexception("Wrong number of players. Expected %u but read %u from packet\n",
				                 static_cast<unsigned>(map.get_nrplayers()),
				                 static_cast<unsigned>(nr_players));
			}
			MapIndex no_of_fields = map.max_index();

			iterate_players_existing(p, nr_players, egbase, player) {
				const unsigned player_no_from_packet = fr.unsigned_8();
				if (p != player_no_from_packet) {
					throw wexception("Wrong player number. Expected %u but read %u from packet\n",
					                 static_cast<unsigned>(p),
					                 static_cast<unsigned>(player_no_from_packet));
				}

				std::set<Player::Field*> seen_fields;

				// Read numerical field infos as combined strings to reduce number of hard disk write
				// operations

				// Some data for all player fields
				// While saving, we iterated all fields for a property and separated the fields by '|'.
				std::vector<std::string> field_vector;

				// Single complex record for a field
				// If a property is multidimensional (e.g. each field has 3 roads), the values are
				// separated by '*'.
				std::vector<std::string> data_vector;
				std::string parseme;

				// field.vision
				parseme = fr.c_string();

				for (MapIndex m = 0; m < no_of_fields; ++m) {
					Player::Field& f = player->fields_[m];
					assert(!f.vision.is_revealed());

					SavedVisionState saved_vision = SavedVisionState(parseme.at(m));
					if (saved_vision == SavedVisionState::kPreviouslySeen) {
						assert(!f.vision.is_visible());
						f.vision = Vision(VisibleState::kPreviouslySeen);
					} else if (saved_vision == SavedVisionState::kRevealed) {
						f.vision.set_revealed(true);
						assert(f.vision.is_revealed());
					} else if (saved_vision == SavedVisionState::kHidden) {
						f.vision.set_hidden(true);
						assert(f.vision.is_hidden());
					}

					if (f.vision.state() == VisibleState::kPreviouslySeen) {
						seen_fields.insert(&f);
					}
				}

				size_t no_of_seen_fields = fr.unsigned_32();

				// Skip data for fields that were never seen, e.g. this happens during saveloading a
				// backup while starting a new game
				if (no_of_seen_fields == 0) {
					continue;
				}

				const size_t additionally_seen = fr.unsigned_32();
				no_of_seen_fields += additionally_seen;
				if (additionally_seen > 0) {
					parseme = fr.c_string();
					split(field_vector, parseme, {'|'});
					assert(field_vector.size() == additionally_seen);
					for (size_t i = 0; i < additionally_seen; ++i) {
						Player::Field& f = player->fields_[stoi(field_vector[i])];
						assert(f.vision.state() == VisibleState::kUnexplored);
						seen_fields.insert(&f);
					}
				}

				if (seen_fields.size() != no_of_seen_fields) {
					throw wexception("Read %" PRIuS
					                 " unseen fields but detected %u when the packet was written\n",
					                 seen_fields.size(), static_cast<unsigned>(no_of_seen_fields));
				}

				// Owner: playernumber|playernumber|playernumber ...
				parseme = fr.c_string();
				split(field_vector, parseme, {'|'});
				assert(field_vector.size() == seen_fields.size());

				size_t counter = 0;
				for (const auto& field : seen_fields) {
					field->owner = stoi(field_vector[counter]);
					++counter;
				}
				assert(counter == no_of_seen_fields);

				// Last Unseen: time|time|time ...
				parseme = fr.c_string();
				split(field_vector, parseme, {'|'});
				assert(field_vector.size() == no_of_seen_fields);

				counter = 0;
				for (const auto& field : seen_fields) {
					field->time_node_last_unseen = Time(stoll(field_vector[counter]));
					++counter;
				}
				assert(counter == no_of_seen_fields);

				// TODO(Nordfriese): Savegame compatibility v1.1
				if (packet_version < 6) {
					fr.c_string();  // Was time_triangle_last_surveyed
					fr.c_string();  // Was resource_amounts
				}

				// Terrains: down*right|down*right|down*right| ...
				parseme = fr.c_string();
				split(field_vector, parseme, {'|'});
				assert(field_vector.size() == no_of_seen_fields);

				counter = 0;
				for (const auto& field : seen_fields) {
					split(data_vector, field_vector[counter], {'*'});
					assert(data_vector.size() == 2);

					field->terrains.store({static_cast<DescriptionIndex>(stoi(data_vector[0])),
					                       static_cast<DescriptionIndex>(stoi(data_vector[1]))});
					++counter;
				}
				assert(counter == no_of_seen_fields);

				// Roads east*southeast*southwest|east*southeast*southwest| ...
				parseme = fr.c_string();
				split(field_vector, parseme, {'|'});
				assert(field_vector.size() == no_of_seen_fields);

				counter = 0;
				for (const auto& field : seen_fields) {
					split(data_vector, field_vector[counter], {'*'});
					assert(data_vector.size() == 3);

					field->r_e = static_cast<Widelands::RoadSegment>(stoi(data_vector[0]));
					field->r_se = static_cast<Widelands::RoadSegment>(stoi(data_vector[1]));
					field->r_sw = static_cast<Widelands::RoadSegment>(stoi(data_vector[2]));
					++counter;
				}
				assert(counter == no_of_seen_fields);

				// Borders: here*right*bottom_right*bottom_left|here*right*bottom_right*bottom_left| ...
				parseme = fr.c_string();
				split(field_vector, parseme, {'|'});
				assert(field_vector.size() == no_of_seen_fields);

				counter = 0;
				for (const auto& field : seen_fields) {
					split(data_vector, field_vector[counter], {'*'});
					assert(data_vector.size() == 4);

					field->border = from_unsigned(stoi(data_vector[0]));
					field->border_r = from_unsigned(stoi(data_vector[1]));
					field->border_br = from_unsigned(stoi(data_vector[2]));
					field->border_bl = from_unsigned(stoi(data_vector[3]));
					++counter;
				}
				assert(counter == no_of_seen_fields);

				// Map objects
				const Descriptions& descriptions = egbase.descriptions();
				for (const auto& field : seen_fields) {
					std::string descr = fr.string();
					if (descr.empty()) {
						field->map_object_descr = nullptr;
					} else {
						// I here assume that no two immovables will have the same internal name
						if (descr == "flag") {
							field->map_object_descr = &g_flag_descr;
						} else if (descr == "portdock") {
							field->map_object_descr = &g_portdock_descr;
						} else {
							std::pair<bool, DescriptionIndex> imm =
							   descriptions.load_building_or_immovable(descr);
							if (imm.first) {
								field->map_object_descr = descriptions.get_building_descr(imm.second);
							} else {
								field->map_object_descr = descriptions.get_immovable_descr(imm.second);
							}
						}

						if (field->map_object_descr->type() == MapObjectType::DISMANTLESITE) {
							field->set_constructionsite(false);
							field->dismantlesite.building = descriptions.get_building_descr(
							   descriptions.safe_building_index(fr.string()));
							field->dismantlesite.progress = fr.unsigned_32();
						} else if (field->map_object_descr->type() == MapObjectType::CONSTRUCTIONSITE) {
							field->set_constructionsite(true);
							field->constructionsite->becomes = descriptions.get_building_descr(
							   descriptions.safe_building_index(fr.string()));
							descr = fr.string();
							field->constructionsite->was =
							   descr.empty() ?
							      nullptr :
							      descriptions.get_building_descr(descriptions.safe_building_index(descr));

							for (uint32_t j = fr.unsigned_32(); j != 0u; --j) {
								field->constructionsite->intermediates.push_back(
								   descriptions.get_building_descr(
								      descriptions.safe_building_index(fr.string())));
							}
							if (packet_version >= 7 && packet_version <= kCurrentPacketVersion) {
								field->constructionsite->progress_64k = fr.unsigned_32();
							} else {
								uint32_t totaltime = fr.unsigned_32();
								uint32_t completedtime = fr.unsigned_32();
								field->constructionsite->progress_64k =
								   totaltime > 0 ? (completedtime << 16) / totaltime : 0;
							}
						}
					}
				}
			}

			iterate_players_existing(p, nr_players, egbase, player) {
				for (MapIndex m = 0; m < no_of_fields; ++m) {
					Player::Field& f = player->fields_[m];
					if (f.vision.state() == VisibleState::kVisible) {
						player->rediscover_node(map, map.get_fcoords(map[m]));
					}
				}
			}

		} else {
			throw UnhandledVersionError("MapPlayersViewPacket", packet_version, kCurrentPacketVersion);
		}
	} catch (const WException& e) {
		throw GameDataError("view: %s", e.what());
	}
}

inline unsigned to_unsigned(bool value) {
	return value ? 1 : 0;
}

void MapPlayersViewPacket::write(FileSystem& fs, EditorGameBase& egbase) {
	FileWrite fw;

	fw.unsigned_16(kCurrentPacketVersion);

	const Map& map = egbase.map();
	const PlayerNumber nr_players = map.get_nrplayers();
	fw.unsigned_8(nr_players);

	iterate_players_existing(p, nr_players, egbase, player) {
		fw.unsigned_8(p);
		std::set<const Player::Field*> seen_fields;
		std::set<const Player::Field*> additionally_seen_fields;

		// Write numerical field infos as combined strings to reduce number of hard disk write
		// operations
		{
			// field.vision
			std::ostringstream oss("");
			const MapIndex upper_bound = map.max_index() - 1;
			for (MapIndex m = 0; m < upper_bound; ++m) {
				const Player::Field& f = player->fields_[m];
				oss << static_cast<char>(f.vision.is_revealed() ? SavedVisionState::kRevealed :
				                         f.vision.is_hidden()   ? SavedVisionState::kHidden :
				                         f.vision.state() == VisibleState::kPreviouslySeen ?
				                                                SavedVisionState::kPreviouslySeen :
				                                                SavedVisionState::kNone);
				if (f.vision.state() == VisibleState::kPreviouslySeen) {
					seen_fields.insert(&f);
					// The data for some of the terrains and edges between PreviouslySeen
					// and Unexplored fields is stored in an Unexplored field. The data
					// for this field therefore needs to be saveloaded as well.
					const Coords coords(m % map.get_width(), m / map.get_width());
					for (const Coords& c : {map.tr_n(coords), map.tl_n(coords), map.l_n(coords)}) {
						const Player::Field& neighbour = player->fields_[map.get_index(c)];
						if (neighbour.vision.state() == VisibleState::kUnexplored) {
							additionally_seen_fields.insert(&neighbour);
						}
					}
				}
			}
			const Player::Field& f = player->fields_[upper_bound];
			oss << static_cast<char>(f.vision.is_revealed() ? SavedVisionState::kRevealed :
			                         f.vision.is_hidden()   ? SavedVisionState::kHidden :
			                         f.vision.state() == VisibleState::kPreviouslySeen ?
			                                                SavedVisionState::kPreviouslySeen :
			                                                SavedVisionState::kNone);
			if (f.vision.state() == VisibleState::kPreviouslySeen) {
				seen_fields.insert(&f);
			}
			fw.c_string(oss.str());
		}

		// Write number of seen fields
		fw.unsigned_32(seen_fields.size());

		// Skip data for fields that were never seen
		if (seen_fields.empty()) {
			assert(additionally_seen_fields.empty());
			continue;
		}

		fw.unsigned_32(additionally_seen_fields.size());
		if (!additionally_seen_fields.empty()) {
			std::ostringstream oss("");
			for (auto it = additionally_seen_fields.begin(); it != additionally_seen_fields.end();) {
				seen_fields.insert(*it);
				const MapIndex index = (*it) - &player->fields_[0];
				oss << index;
				++it;
				if (it != additionally_seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}

		{
			// Owner
			std::ostringstream oss("");
			for (auto it = seen_fields.begin(); it != seen_fields.end();) {
				oss << static_cast<unsigned>((*it)->owner);
				++it;
				if (it != seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}
		{
			// Last Unseen
			std::ostringstream oss("");
			for (auto it = seen_fields.begin(); it != seen_fields.end();) {
				oss << static_cast<unsigned>((*it)->time_node_last_unseen.get());
				++it;
				if (it != seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}
		{
			// Terrains
			std::ostringstream oss("");
			for (auto it = seen_fields.begin(); it != seen_fields.end();) {
				oss << (*it)->terrains.load().d << "*" << (*it)->terrains.load().r;
				++it;
				if (it != seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}
		{
			// Roads
			std::ostringstream oss("");
			for (auto it = seen_fields.begin(); it != seen_fields.end();) {
				oss << static_cast<unsigned>((*it)->r_e) << "*" << static_cast<unsigned>((*it)->r_se)
				    << "*" << static_cast<unsigned>((*it)->r_sw);
				++it;
				if (it != seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}
		{
			// Borders
			std::ostringstream oss("");
			for (auto it = seen_fields.begin(); it != seen_fields.end();) {
				oss << to_unsigned((*it)->border) << "*" << to_unsigned((*it)->border_r) << "*"
				    << to_unsigned((*it)->border_br) << "*" << to_unsigned((*it)->border_bl);
				++it;
				if (it != seen_fields.end()) {
					oss << "|";
				}
			}
			fw.c_string(oss.str());
		}

		// Map objects
		for (const auto& field : seen_fields) {
			if (field->map_object_descr != nullptr) {
				fw.string(field->map_object_descr->name());

				if (field->map_object_descr->type() == MapObjectType::DISMANTLESITE) {
					if (const BuildingDescr* b = field->dismantlesite.building; b != nullptr) {
						fw.string(b->name());
						fw.unsigned_32(field->dismantlesite.progress);
					} else {
						// `building` was only supposed to be nullptr in compatibility cases.
						// Not throw because we may already be doing and emergency save.
						log_err("Previously seen dismantlesite has no building type");
						// Let's pretend we didn't have an immovable...
						fw.string("");
					}
				} else if (field->map_object_descr->type() == MapObjectType::CONSTRUCTIONSITE) {
					fw.string(field->constructionsite->becomes->name());
					fw.string(field->constructionsite->was != nullptr ?
					             field->constructionsite->was->name() :
					             "");

					fw.unsigned_32(field->constructionsite->intermediates.size());
					for (const BuildingDescr* d : field->constructionsite->intermediates) {
						fw.string(d->name());
					}

					fw.unsigned_32(field->constructionsite->progress_64k);
				}
			} else {
				fw.string("");
			}
		}
	}

	fw.write(fs, "binary/view");
}
}  // namespace Widelands
