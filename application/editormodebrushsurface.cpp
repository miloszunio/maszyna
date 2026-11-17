/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the brush of the Surroundings putting 3D grass (the blades the shader of the terrain draws) or the ground texture (the
// materials of the terrain palette) instead of models: in a circle while the button is held, the grass also in a band along a
// path clicked point by point

#include "stdafx.h"

#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "editor/editorFormat.hpp"
#include "rendering/renderer.h"
#include "utilities/Globals.h"
#include "utilities/translation.h"

#include <algorithm>

namespace
{

using target = brush_object_list::target;

// a hint over the settings, wrapped to the width of the window
void hint(char const *Text)
{
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextDisabled("%s", Text);
	ImGui::PopTextWrapPos();
}

void tooltip(char const *Text)
{
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Text);
}

} // namespace

bool editor_mode::brush_puts_surface() const
{
	return ui()->mode() == nodebank_panel::BRUSH && ui()->brush().what != target::models;
}

bool editor_mode::brush_grass_path() const
{
	auto const &settings{ui()->brush()};
	return brush_puts_surface() && settings.what == target::grass && settings.grass_stroke == brush_object_list::grass_shape::path;
}

bool editor_mode::brush_surface_painting() const
{
	return mouseHold && false == m_brush_cancelled && brush_puts_surface() && false == brush_grass_path() && m_streamer.active();
}

void editor_mode::brush_surface_press()
{
	if (false == m_streamer.active())
	{
		m_brush_cancelled = true;
		ui()->set_status(STR_C("Brush: the scenery has no terrain of the Terrain mode to grow the 3D grass on or to paint"), true);
		return;
	}
	// Alt+LMB picks what's under the cursor, and the button held then doesn't paint
	if (Global.altState)
	{
		m_brush_cancelled = true;
		brush_pick_under_cursor();
		return;
	}
	if (brush_grass_path())
	{
		// a point of the path, read once the renderer has the depth under the cursor
		GfxRenderer->Pick_Node_Callback([this](scene::basic_node * /*node*/) { brush_line_point(); });
		return;
	}
	// the stroke in the circle goes on in the update while the button is held, one step of the history (terrain_history_step)
}

void editor_mode::brush_surface_stroke(double const Deltatime)
{
	// the ground itself along the ray through the cursor in this frame, as the brushes of the Terrain mode have it
	glm::dvec3 world;
	if (false == terrain_cursor(world) || m_streamer.terrain_at(world.x, world.z) == nullptr)
		return;
	auto const rate{m_terrain_brush_strength * Deltatime};
	if (ui()->brush().what == target::texture)
	{
		// mixes the material in, covers with it at once, or takes the paint back to the first material (Shift too)
		auto const layer{(Global.shiftState || m_paint_mode == 2) ? std::uint16_t{0} : static_cast<std::uint16_t>(m_terrain_layer)};
		m_streamer.paint(world.x, world.z, m_terrain_brush_radius, m_paint_mode == 1 ? 1.0 : std::min(1.0, rate * 0.25), layer);
		return;
	}
	if (m_streamer.grass_kinds().empty() && false == Global.shiftState)
	{
		// the first kind of the presets, so the brush grows something
		m_streamer.add_grass_kind(heightmap::grass_presets().front());
		m_grass_kind = 0;
	}
	m_grass_kind = std::clamp(m_grass_kind, 0, std::max(0, static_cast<int>(m_streamer.grass_kinds().size()) - 1));
	m_streamer.grass(world.x, world.z, m_terrain_brush_radius, std::min(1.0, rate * 0.25), static_cast<std::uint16_t>(m_grass_kind), Global.shiftState);
}

void editor_mode::brush_grass_along(bool const Erase)
{
	if (m_brush_line.size() < 2)
	{
		ui()->set_status(STR_C("3D grass: a path needs two points at least"));
		return;
	}
	if (false == m_streamer.active())
	{
		m_brush_line.clear();
		ui()->set_status(STR_C("Brush: the scenery has no terrain of the Terrain mode to grow the 3D grass on or to paint"), true);
		return;
	}
	if (m_streamer.grass_kinds().empty() && false == Erase)
	{
		m_streamer.add_grass_kind(heightmap::grass_presets().front());
		m_grass_kind = 0;
	}
	m_grass_kind = std::clamp(m_grass_kind, 0, std::max(0, static_cast<int>(m_streamer.grass_kinds().size()) - 1));
	auto const kind{static_cast<std::uint16_t>(m_grass_kind)};
	auto const &line{m_brush_line};
	auto const &band{m_grass_band};
	// the whole path one step of the history
	terrain_history_step();
	bool changed{false};
	// piece by piece, each place grown by the piece nearest to it alone, so where the pieces meet it isn't grown twice
	for (std::size_t piece = 0; piece + 1 < line.size(); ++piece)
	{
		auto const [low, high]{scatter::piece_bounds(line, piece, band)};
		if (m_streamer.grass_where(low, high, kind, Erase, [&line, &band, piece](double const X, double const Z) {
			    auto const [nearest, distance]{scatter::nearest_piece(line, {X, Z})};
			    return nearest == piece ? scatter::band_amount(band, distance) : 0.0;
		    }))
			changed = true;
	}
	terrain_history_step();
	m_brush_line.clear();
	if (false == changed)
		ui()->set_status(Erase ? STR_C("3D grass: no grass along the path to take away") : STR_C("3D grass: nothing grown, no chunk of the terrain under the path, or no room for another kind of grass"));
	else if (Erase)
		ui()->set_status(STR_C("3D grass taken away along the path; Ctrl+Z puts it back"));
	else
		ui()->set_status(format(STR_C("3D grass grown along the path: %s; Ctrl+Z takes it back"), m_streamer.grass_kinds()[kind].name.c_str()));
}

bool editor_mode::grass_at_cursor(std::uint16_t &Kind) const
{
	glm::dvec3 world;
	if (false == terrain_cursor(world))
		return false;
	auto const *chunk{m_streamer.terrain_at(world.x, world.z)};
	if (chunk == nullptr)
		return false;
	auto const &data{chunk->data()};
	auto const count{data.grass_count()};
	auto const samples{data.grass};
	if (samples <= 0 || count == 0)
		return false;
	auto const plane{static_cast<std::size_t>(samples + 1) * static_cast<std::size_t>(samples + 1)};
	if (data.grass_density.size() < count * plane)
		return false;
	auto const low{data.corner()};
	auto const step{heightmap::chunk_size / samples};
	auto const ix{std::clamp(static_cast<int>(std::lround((world.x - low.x) / step)), 0, samples)};
	auto const iz{std::clamp(static_cast<int>(std::lround((world.z - low.y) / step)), 0, samples)};
	auto const sample{static_cast<std::size_t>(iz) * static_cast<std::size_t>(samples + 1) + static_cast<std::size_t>(ix)};
	int densest{0};
	for (std::size_t slot = 0; slot < count; ++slot)
	{
		if (int const density{data.grass_density[slot * plane + sample]}; density > densest)
		{
			densest = density;
			Kind = data.grass_kinds[slot];
		}
	}
	return densest > 0;
}

bool editor_mode::brush_pick_under_cursor()
{
	if (ui()->brush().what == target::grass)
	{
		std::uint16_t kind{0};
		if (false == grass_at_cursor(kind) || kind >= m_streamer.grass_kinds().size())
		{
			ui()->set_status(STR_C("3D grass: no grass under the cursor to pick"));
			return false;
		}
		m_grass_kind = kind;
		m_grass_edit_of = -1;
		ui()->set_status(format(STR_C("3D grass: picked %s"), m_streamer.grass_kinds()[kind].name.c_str()));
		return true;
	}
	std::vector<std::pair<std::uint16_t, int>> weights;
	std::size_t used{0};
	auto const &layers{m_streamer.manifest().layers};
	if (false == paint_at_cursor(weights, used) || weights.empty() || weights.front().first >= layers.size())
	{
		ui()->set_status(STR_C("Ground texture: no terrain under the cursor to pick a material from"));
		return false;
	}
	m_terrain_layer = weights.front().first;
	ui()->set_status(format(STR_C("Ground texture: picked %s"), layers[m_terrain_layer].material.c_str()));
	return true;
}

bool editor_mode::brush_escape()
{
	// the line or the path being clicked dropped
	if (brush_on_line() && false == m_brush_line.empty())
	{
		m_brush_line.clear();
		ui()->set_status(brush_puts_surface() ? STR_C("3D grass: the path dropped") : STR_C("Brush: the line dropped"));
		return true;
	}
	if (false == mouseHold || m_brush_cancelled)
		return false;
	if (m_brush_stroke)
	{
		// the models of the stroke so far taken back, and no more put until the button is let go
		end_brush_stroke();
		m_brush_cancelled = true;
		if (m_history.size() > m_brush_history_mark)
		{
			undo_last();
			g_redo.clear();
		}
		ui()->set_status(STR_C("Brush: the stroke taken back (Esc)"));
		return true;
	}
	if (false == brush_surface_painting())
		return false;
	// the grass or the paint of the stroke so far a step of its own, taken back
	m_brush_cancelled = true;
	terrain_history_step();
	if (m_history.size() > m_brush_history_mark && m_history.back().action == EditorSnapshot::Action::Terrain)
	{
		undo_last();
		g_redo.clear();
	}
	ui()->set_status(STR_C("Brush: the stroke taken back (Esc)"));
	return true;
}

void editor_mode::render_brush_surface_options()
{
	if (false == m_streamer.active())
	{
		ImGui::TextWrapped("%s", STR_C("The scenery has no terrain of the Terrain mode yet: there's nothing to grow the 3D grass on or to paint the ground texture of. "
		                               "Make it in the Terrain mode, from the terrain model or an empty, flat one."));
		return;
	}
	auto const texture{ui()->brush().what == target::texture};
	if (brush_grass_path())
	{
		hint(STR_C("LMB: the points of the path. Enter grows the grass along it, Shift+Enter takes the grass away along it. "
		           "Backspace takes the last point back, Esc drops the path. Alt+LMB picks the kind of grass under the cursor"));
		if (auto width{static_cast<float>(m_grass_band.width)}; ImGui::SliderFloat(STR_C("Width each side##grasspath"), &width, 0.1f, 50.0f, "%.1f m", ImGuiSliderFlags_Logarithmic))
			m_grass_band.width = width;
		tooltip(STR_C("How far to each side of the path the grass grows fully"));
		if (auto feather{static_cast<float>(m_grass_band.feather)}; ImGui::SliderFloat(STR_C("Soft edge##grasspath"), &feather, 0.0f, 20.0f, "%.1f m"))
			m_grass_band.feather = feather;
		tooltip(STR_C("Metres beyond the width over which the grass thins out to nothing; 0: a sharp edge"));
		if (auto amount{static_cast<float>(m_grass_band.amount * 100.0)}; ImGui::SliderFloat(STR_C("Fullness##grasspath"), &amount, 1.0f, 100.0f, "%.0f%%"))
			m_grass_band.amount = amount / 100.0;
		tooltip(STR_C("100%: the full density of the kind (blades/m2 in the palette below), the other kinds there giving way;\n"
		              "less: part of the way there, the grass growing there already partly kept. With Shift+Enter: how much of the grass is taken away"));
		if (false == m_brush_line.empty())
			ImGui::TextDisabled(STR_C("Points of the path: %zu"), m_brush_line.size());
	}
	else
	{
		hint(texture ? STR_C("Hold LMB: paints the material chosen below in the circle. Shift+LMB: the first material of the palette. "
		                     "Alt+LMB: picks the material under the cursor. Esc with the button held takes the stroke back")
		             : STR_C("Hold LMB: grows the kind of grass chosen below in the circle. Shift+LMB: takes all grass away. "
		                     "Alt+LMB: picks the kind of grass under the cursor. Esc with the button held takes the stroke back"));
		ImGui::SliderFloat(STR_C("Radius##brushsurface"), &m_terrain_brush_radius, 0.5f, 100.0f, "%.1f m", ImGuiSliderFlags_Logarithmic);
		m_terrain_brush_radius = std::max(0.5f, m_terrain_brush_radius);
		tooltip(STR_C("The radius of the brush, the same as of the brushes of the Terrain mode"));
		ImGui::SliderFloat(STR_C("Strength##brushsurface"), &m_terrain_brush_strength, 0.05f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		m_terrain_brush_strength = std::max(0.05f, m_terrain_brush_strength);
		tooltip(texture ? STR_C("How fast the material covers the ground while the button is held (Mix); Cover covers at once")
		                : STR_C("How fast the grass grows dense while the button is held"));
	}
	if (texture)
	{
		ImGui::SeparatorText(STR_C("Materials##brushtexture"));
		render_materials_ui();
		return;
	}
	ImGui::SeparatorText(STR_C("Kinds of 3D grass##brushgrass"));
	render_grass_ui();
}

void editor_mode::draw_brush_surface_cursor()
{
	if (brush_grass_path())
	{
		glm::dvec3 at;
		auto const on_ground{brush_ground(at)};
		draw_brush_line(on_ground ? &at : nullptr, m_grass_band.width, m_grass_band.feather);
		if (on_ground)
			draw_brush_cross(at, 1.0);
		return;
	}
	glm::dvec3 centre;
	if (false == m_streamer.active() || false == terrain_cursor(centre))
		return;
	draw_brush_cross(centre, std::clamp(0.1 * m_terrain_brush_radius, 0.5, 3.0));
	draw_brush_circle(centre, m_terrain_brush_radius);
}
