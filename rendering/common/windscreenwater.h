/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "gl/ubo.h"
#include "interfaces/ITexture.h"
#include "utilities/Classes.h"

struct opengl_material;
class TModel3d;
class TSubModel;
class TDynamicObject;
class TMoverParameters;

namespace gfx
{

// whether the windscreens of the cab the train is driven from are heated (TMoverParameters::windows_heated()); true without one
bool cab_windows_heated();
// what falls on the windscreens, as the block of the scene gives it to mat_rain_windscreen (rain_params.z): 0 rain, 1 snow, 2
// snow on windscreens which aren't heated
float glass_precipitation();

// the water on the windscreens of the cab of the train, simulated on the gpu, whichever api runs it.
//
// a glass is a material of mat_rain_windscreen (one with a mask of the wipers) in the model of the cab of the train, up to
// gl::WATER_SLOTS of them; the material is told its slot in its param water_slot (1 + the slot, the drawing of the glass reads
// the water of the slot where it's there, mat_rain_windscreen.frag). the water lies in the coordinates of the texture of the glass
// (uv), as the mask does. the map of a glass is made here from the triangles of the model with its material: for the texels of
// the uv on the glass, the step along the glass (metres, the space of the model of the cab) for a step of u and of v, and which
// way is out of the cab; the passes of the gpu bake the mask of the wipers into it. once a frame the step of the simulation is
// worked out here (the time, the rain or snow, gravity and the air against the cab in the space of its model, the blades) and
// the passes run it (water_*.comp: the landing drops, the drops moving and carried by the blades, the grids, the drops flowing
// together); what they keep and the glasses read is the storage of the water (water.glsl, gl::WATER_*_OFFSET)
class windscreen_water
{
  public:
	// what the renderer gives: the materials of its manager, which get their slots
	struct host
	{
		std::function<opengl_material *(material_handle)> material;
		// a material made from the text of a .mat (gfx.windscreen.auto: the glasses found by their shape get one of rain_windscreen)
		std::function<material_handle(std::string const &, std::string const &)> create;
	};

	windscreen_water() = default;
	virtual ~windscreen_water() = default;
	windscreen_water(windscreen_water const &) = delete;
	windscreen_water &operator=(windscreen_water const &) = delete;

	void set_host(host Host)
	{
		m_host = std::move(Host);
	}
	// whether the passes could be made
	bool available();
	// once a frame, before the colour passes: the cab of the train followed, the step of the simulation run. Seconds: of the
	// simulation since the frame before (0 when it's paused)
	void update(double Seconds);
	// the materials let go of their slots, the glasses forgotten (the storage stays as it is, unused)
	void release();
	std::string info() const;

  protected:
	// the glasses forgotten and the programs taken as not made yet (the renderer let go of them), made again when next needed
	void reset();
	// the programs and the storage; false: the simulation can't be used, the glasses make their water up
	virtual bool gpu_init() = 0;
	// the slot set up for a glass: its drops, its grid and its paths cleared, its header saying it isn't simulated (until the
	// first step), the axes of its map (two vec4 a texel, gl::WATER_MAP x WATER_MAP texels, see water.glsl) put in the storage
	virtual void gpu_setup(std::size_t Slot, std::vector<glm::vec4> const &Axes) = 0;
	// the mask of the wipers of a glass (a texture of the renderer) baked into its map; false when the texture isn't ready yet
	// (tried again with the next frame)
	virtual bool gpu_bake(std::size_t Slot, texture_handle Mask, gl::water_ubs const &Block) = 0;
	// the mask of the wipers of a glass worked out from its shape put in its map in place of a baked one (gl::WATER_MAP x WATER_MAP
	// texels, a blade a channel, as the bake leaves it)
	virtual void gpu_blades(std::size_t Slot, std::vector<glm::vec4> const &Blades) = 0;
	// the step of the simulation of the glasses Block says are simulated; Tries: the most landings tried on a glass
	virtual void gpu_step(gl::water_ubs const &Block, std::uint32_t Tries) = 0;

  private:
	struct glass
	{
		material_handle material{null_handle};
		texture_handle mask{null_handle};
		float densest{0.f}; // the most metres squared a unit of the uv holds
		float area{0.f}; // metres squared of the glass
		double landings{0.0}; // the share of a landing left over from the steps before
		bool baked{false};
		bool found{false}; // found by its shape (gfx.windscreen.auto): its mask of the wipers is worked out, not baked
	};
	// what a glass found by its shape keeps until its wipers are worked out: the place in the model of the cab of each texel of
	// its map, and whether the texel is on the glass
	struct found_glass
	{
		std::size_t slot{0};
		std::vector<glm::vec3> places;
		std::vector<bool> on;
		// each texel on the glass in the plane of the glass, metres across it and up it, worked out from the map (the steps along
		// the glass for the steps of u and v) rather than from the places: two windscreens sharing their texture lie on the same
		// texels, and the places of those are of whichever was laid last
		std::vector<glm::vec2> flat;
		bool planar{false};
	};

	// the glasses of the model of a cab found and set up
	void build(TModel3d &Cab);
	// the triangles of a glass laid into its map; Inside: a point inside the cab. Places: where each texel lies in the model of the
	// cab, when wanted
	void rasterize(TSubModel *Submodel, material_handle Material, glm::vec3 const &Inside, std::vector<glm::vec4> &Axes, std::vector<glm::vec3> *Places, glm::vec4 const &Outer) const;
	// gfx.windscreen.auto: the glasses of a cab which has none of rain_windscreen, told by their shape (large translucent surfaces
	// on the outside of the cab facing ahead or back), given a material of rain_windscreen with their own texture. Lower, Upper:
	// the box around the model. returns the materials given
	std::vector<material_handle> find_glasses(TSubModel *Root, glm::vec3 const &Lower, glm::vec3 const &Upper, float Least);
	// the windscreens of the model of the vehicle seen from outside found the same way and given the material of rain_windscreen:
	// not simulated, they draw the rain and the snow made up by the material, as the weather is, so the glass isn't clean outside
	// while it's covered seen from the cab
	void dress_outside(TModel3d &Model);
	// the material of rain_windscreen for a glass of the material Source, one of each Kind (the windscreens, the side windows);
	// null_handle when it can't be made
	material_handle glass_material(material_handle Source, std::string const &Kind);
	// the wipers of the vehicle laid over the glasses found by their shape: each blade a pivot under the bottom edge of its glass and
	// an arc it sweeps, put in the maps of the glasses as the masks of the wipers
	void lay_wipers(std::vector<found_glass> const &Glasses);
	// the place of the virtual wipers over their sweep, 0..1, stepped by Seconds
	float virtual_wipers(TMoverParameters const &Mover, double Seconds);
	// the slots given to the materials of the glasses (every frame: a material made anew takes its default params)
	void assign(bool Assigned);

	host m_host;
	texture_handle m_nowipers{null_handle}; // the mask of the materials made for the glasses found by their shape: no blade anywhere
	int m_state{0}; // 0: not set up yet, 1: ready, -1: can't be used
	TDynamicObject const *m_vehicle{nullptr};
	TModel3d const *m_cab{nullptr};
	std::array<glass, gl::WATER_SLOTS> m_glasses{};
	std::size_t m_glasscount{0};
	glm::vec4 m_bladesbefore{-1.f};
	double m_time{0.0}; // seconds the simulation runs
	std::vector<TModel3d const *> m_dressed; // the models of the vehicles seen from outside dressed already
	bool m_virtualwipers{false};
	std::size_t m_virtualblades{0}; // one on each windscreen found by its shape
	// the glasses found by their shape: the windscreens (true) and the side windows (false); only their triangles at the outside of
	// the cab are laid into their maps, the glasses of the gauges of the same submodel left out
	std::unordered_map<material_handle, bool> m_foundahead; // the glasses found by their shape on a vehicle without wipers have virtual ones
	double m_wiperphase{0.0}; // of their sweep there and back, 0..1
	std::uint32_t m_step{0};
	std::uint32_t m_tries{0}; // landings tried in the last step, all glasses
};

} // namespace gfx
