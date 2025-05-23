/*
 * Copyright (C) 2002-2025 by the Widelands Development Team
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

#ifndef WL_EDITOR_TOOLS_NOISE_HEIGHT_TOOL_H
#define WL_EDITOR_TOOLS_NOISE_HEIGHT_TOOL_H

#include "editor/tools/set_height_tool.h"

/// Set the height of a node to a random value within a defined interval.
struct EditorNoiseHeightTool : public EditorTool {
	explicit EditorNoiseHeightTool(
	   EditorInteractive& parent,
	   EditorSetHeightTool& the_set_tool,
	   const Widelands::HeightInterval& the_interval = Widelands::HeightInterval(10, 14))
	   : EditorTool(parent, the_set_tool, the_set_tool),
	     set_tool_(the_set_tool),
	     interval_(the_interval) {
	}

	int32_t handle_click_impl(const Widelands::NodeAndTriangle<>& center,
	                          EditorActionArgs* args,
	                          Widelands::Map* map) override;

	int32_t handle_undo_impl(const Widelands::NodeAndTriangle<>& center,
	                         EditorActionArgs* args,
	                         Widelands::Map* map) override;

	EditorActionArgs format_args_impl() override;

	[[nodiscard]] const Image* get_sel_impl() const override {
		return g_image_cache->get("images/wui/editor/fsel_editor_noise_height.png");
	}

	[[nodiscard]] Widelands::HeightInterval get_interval() const {
		return interval_;
	}
	void set_interval(const Widelands::HeightInterval& i) {
		interval_ = i;
	}

	[[nodiscard]] EditorSetHeightTool& set_tool() const {
		return set_tool_;
	}

	WindowID get_window_id() override {
		return WindowID::NoiseHeight;
	}

	bool save_configuration_impl(ToolConf& conf) override {
		conf.interval = interval_;
		return true;
	}
	void load_configuration(const ToolConf& conf) override {
		interval_ = conf.interval;
	}
	std::string format_conf_description_impl(const ToolConf& conf) override;

private:
	EditorSetHeightTool& set_tool_;
	Widelands::HeightInterval interval_;
};

#endif  // end of include guard: WL_EDITOR_TOOLS_NOISE_HEIGHT_TOOL_H
