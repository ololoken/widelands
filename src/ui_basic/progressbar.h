/*
 * Copyright (C) 2004-2025 by the Widelands Development Team
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

#ifndef WL_UI_BASIC_PROGRESSBAR_H
#define WL_UI_BASIC_PROGRESSBAR_H

#include "graphic/styles/progress_bar_style.h"
#include "ui_basic/panel.h"

namespace UI {

/**
 * A very simple panel that displays progress or productivity in percent,
 * graphically enhanced with a coloured bar.
 *
 * The actual state of progress
 */
struct ProgressBar : public Panel {
	enum {
		Horizontal = 0,  ///< from left to right
		Vertical = 1,    ///< from bottom to top

		DefaultWidth = 100,
		DefaultHeight = 24,
	};

public:
	ProgressBar(Panel* parent,
	            PanelStyle,
	            const std::string& name,
	            int32_t x,
	            int32_t y,
	            int32_t w,
	            int32_t h,
	            uint32_t orientation);

	[[nodiscard]] uint32_t get_state() const {
		return state_;
	}
	void set_state(uint32_t);
	[[nodiscard]] uint32_t get_total() const {
		return total_;
	}
	void set_total(uint32_t);

	void set_show_percent(bool p) {
		show_percent_ = p;
	}
	[[nodiscard]] bool get_show_percent() const {
		return show_percent_;
	}

protected:
	void draw(RenderTarget&) override;

private:
	uint32_t orientation_;
	uint32_t state_{0U};    ///< state_ is [0..total_]
	uint32_t total_{100U};  ///< maximum progress
	const UI::PanelStyle progress_style_;
	const UI::ProgressbarStyleInfo& progress_style() const;
	bool show_percent_{true};
};
}  // namespace UI

#endif  // end of include guard: WL_UI_BASIC_PROGRESSBAR_H
