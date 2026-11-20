/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the snow lying around the camera in the vulkan renderer (environment/snowcover.h, Global.gfx_snow; opengl46_snow): every frame the
// drifts are worked out on the gpu from the surface of the scenery the maps of the occlusion of the sky hold (snow_drift.comp, then
// snow_settle.comp, with the commands of the frame before its passes, outside a rendering) into a buffer of cells, which the pass
// sets hold for the shading (SNOW_SSBO, shaders/vk14/snow.glsl): it puts the snow on the surfaces by them, and the surface of the
// drifts is drawn over the ground with them (snow_surface.*). while there are no cells (no snow, no maps: gfx.skyvisibility no)
// the pass sets hold a stand-in of one cell, the snow lies evenly on the surfaces turned up, and there are no drifts

#pragma once

#include "rendering/vulkan14/vulkan14compute.h"

#include <functional>
#include <memory>
#include <string>

namespace vk14
{

class sky_occlusion_maps;

class snow_drifts
{
  public:
	// what the renderer does for the snow
	struct host
	{
		// the pass set of the scene bound for a kernel, with the blocks the frame wrote last (the weather)
		std::function<void(compute_kernel const &Kernel)> bind;
	};

	snow_drifts() = default;
	~snow_drifts();
	snow_drifts(snow_drifts const &) = delete;
	snow_drifts &operator=(snow_drifts const &) = delete;

	// called once, before the pass sets are made: the stand-in of the cells made (the pass sets hold it until there are cells).
	// Nearest: the sampler the kernels read the maps with, texel by texel. false: no stand-in
	bool init(VkDevice Device, pipeline_library &Pipelines, shader_library &Shaders, VkDescriptorSetLayout Passlayout, allocator &Memory, deletion_queue &Deletions, VkSampler Nearest, host Host);
	// everything goes (the device is idle)
	void destroy();
	// the value of the timeline the gpu reaches when it's done with the frame being recorded
	void begin_frame(std::uint64_t const Value)
	{
		m_value = Value;
	}
	// whether the drifts are to be worked out in this frame: the snow lies and is drawn, and the maps hold the surface it lies on
	bool wanted(sky_occlusion_maps const &Maps) const;
	// the drifts of the frame, from the maps as they are after their flush and the weather block of the frame, worked out with
	// Commands (outside a rendering). false: nothing was dispatched, and there are no drifts in this frame
	bool update(command_stream *Commands, sky_occlusion_maps const &Maps, glm::dvec3 const &Camera);
	// no drifts until the next update (a scenery is being loaded)
	void rest()
	{
		m_active = false;
	}
	// what the pass sets hold: the cells of the frame, or the stand-in
	VkBuffer cells() const;
	// whether the surface of the drifts is to be drawn in this frame
	bool drawn() const;
	// the texels across the grid of the surface (fewer than across the maps, which reach further than they're filled)
	int across() const
	{
		return m_across;
	}
	// line for the statistics of the debug panel
	std::string info() const;

  private:
	// the kernels made when the snow first lies; false when they can't be
	bool ready();
	void release();
	bool allocate(int Side);
	// the kernels dispatched; false when the frame gets no drifts
	bool work(command_stream *Commands, sky_occlusion_maps const &Maps, glm::dvec3 const &Camera);

	VkDevice m_device{VK_NULL_HANDLE};
	pipeline_library *m_pipelines{nullptr};
	shader_library *m_shaders{nullptr};
	VkDescriptorSetLayout m_passlayout{VK_NULL_HANDLE};
	allocator *m_memory{nullptr};
	deletion_queue *m_deletions{nullptr};
	VkSampler m_nearest{VK_NULL_HANDLE};
	host m_host;
	std::uint64_t m_value{0};
	int m_state{0}; // 0: the kernels not made yet, 1: ready, -1: can't be used
	compute_kernel m_drift;
	compute_kernel m_settle;
	buffer m_standin; // a cell with nothing known, read by no pass
	std::shared_ptr<buffer> m_cells; // four floats each
	std::shared_ptr<image> m_drifted; // what snow_drift.comp gives snow_settle.comp, two floats a texel
	int m_side{0};
	int m_across{0};
	bool m_active{false};
};

} // namespace vk14
