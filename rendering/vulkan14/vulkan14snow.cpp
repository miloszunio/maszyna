/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "rendering/vulkan14/vulkan14snow.h"

#include "gl/ubo.h"
#include "rendering/vulkan14/vulkan14commands.h"
#include "rendering/vulkan14/vulkan14passes.h"
#include "rendering/vulkan14/vulkan14skyocclusion.h"
#include "environment/weather.h"
#include "simulation/simulationenvironment.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"

#include <cmath>
#include <cstring>
#include <format>

namespace vk14
{

namespace
{

VkPipelineStageFlags2 const compute_stage{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT};

// the push constants of the kernels, laid out as those of the shaders
struct drift_constants
{
	// x: texels across the maps, yz: the texel of the camera the maps follow, toroidal
	glm::ivec4 map;
	// x: m of a texel, y: m from the camera the working of the wind fades out from, z: m it fades over
	glm::vec4 geometry;
};
static_assert(sizeof(drift_constants) == 32, "the layout of the constants of snow_drift.comp");
struct settle_constants
{
	// x: texels across the maps
	glm::ivec4 map;
};
static_assert(sizeof(settle_constants) == 16, "the layout of the constants of snow_settle.comp");

std::uint32_t groups(int const Size)
{
	return static_cast<std::uint32_t>((Size + static_cast<int>(gl::SNOW_GROUP_SIZE) - 1) / static_cast<int>(gl::SNOW_GROUP_SIZE));
}

double wrap(double const Value, double const Size)
{
	auto const result{std::fmod(Value, Size)};
	return (result < 0.0) ? result + Size : result;
}

// the snow is there to be drawn at all
bool snowing_scenery()
{
	auto const &snow{simulation::Environment.weather().snow()};
	return (Global.gfx_snow > 0) && ((snow.depth() > 0.0005f) || (snow.plaster() > 0.01f));
}

} // namespace

snow_drifts::~snow_drifts()
{
	destroy();
}

bool snow_drifts::init(VkDevice const Device, pipeline_library &Pipelines, shader_library &Shaders, VkDescriptorSetLayout const Passlayout, allocator &Memory, deletion_queue &Deletions, VkSampler const Nearest, host Host)
{
	m_device = Device;
	m_pipelines = &Pipelines;
	m_shaders = &Shaders;
	m_passlayout = Passlayout;
	m_memory = &Memory;
	m_deletions = &Deletions;
	m_nearest = Nearest;
	m_host = std::move(Host);
	// a binding of a set with no buffer to read from is undefined, also where the shading doesn't read it
	if ((false == m_standin.create(Memory, sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memory_kind::upload, "snow cells stand-in")) || (m_standin.mapped() == nullptr))
	{
		m_standin.destroy();
		return false;
	}
	glm::vec4 const none{-1.0e4f, 0.f, 1.0e4f, 0.f};
	std::memcpy(m_standin.mapped(), &none, sizeof(none));
	m_standin.flush(0, sizeof(none));
	return true;
}

void snow_drifts::destroy()
{
	if (m_device == VK_NULL_HANDLE)
	{
		return;
	}
	// the device is idle: everything goes at once
	m_deletions = nullptr;
	release();
	m_drift.destroy(m_device);
	m_settle.destroy(m_device);
	m_standin.destroy();
	m_state = 0;
	m_device = VK_NULL_HANDLE;
}

bool snow_drifts::ready()
{
	if (m_state != 0)
	{
		return m_state == 1;
	}
	m_state = -1;
	if ((m_device == VK_NULL_HANDLE) || (false == bool(m_host.bind)))
	{
		return false;
	}
	auto made{false};
	try
	{
		made = m_drift.create(m_device, *m_pipelines, m_shaders->get("snow_drift.comp"), m_passlayout, {}, "snow_drift.comp") &&
		       m_settle.create(m_device, *m_pipelines, m_shaders->get("snow_settle.comp"), m_passlayout, {}, "snow_settle.comp");
		if (false == made)
		{
			ErrorLog("snow: no compute pipelines of the drifts, the snow lies evenly and without drifts");
		}
	}
	catch (gfx::shader_exception const &e)
	{
		ErrorLog("snow: the shaders can't be used, the snow lies evenly and without drifts (" + std::string(e.what()) + ")");
	}
	if (made)
	{
		m_state = 1;
	}
	return made;
}

void snow_drifts::release()
{
	if ((m_deletions != nullptr) && (m_cells || m_drifted))
	{
		// the frames in flight may still read them
		m_deletions->push(m_value, [cells = std::move(m_cells), drifted = std::move(m_drifted)]() {});
	}
	m_cells.reset();
	m_drifted.reset();
	m_side = 0;
	m_across = 0;
	m_active = false;
}

bool snow_drifts::allocate(int const Side)
{
	release();
	auto cells{std::make_shared<buffer>()};
	auto const size{static_cast<VkDeviceSize>(Side) * static_cast<VkDeviceSize>(Side) * sizeof(glm::vec4)};
	image_desc desc;
	desc.format = VK_FORMAT_R32G32_SFLOAT;
	desc.extent = {static_cast<std::uint32_t>(Side), static_cast<std::uint32_t>(Side), 1};
	desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	auto drifted{std::make_shared<image>()};
	if ((false == cells->create(*m_memory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memory_kind::device, "snow cells")) || (false == drifted->create(*m_memory, desc, "snow drifted")))
	{
		ErrorLog(std::format("snow: no memory for the drifts over {0}x{0} cells, the snow lies evenly", Side));
		return false;
	}
	m_cells = std::move(cells);
	m_drifted = std::move(drifted);
	m_side = Side;
	WriteLog(std::format("snow: the drifts worked out over {0}x{0} cells", Side));
	return true;
}

bool snow_drifts::wanted(sky_occlusion_maps const &Maps) const
{
	return (m_state >= 0) && snowing_scenery() && Maps.active() && (Maps.surface() != nullptr);
}

bool snow_drifts::update(command_stream *const Commands, sky_occlusion_maps const &Maps, glm::dvec3 const &Camera)
{
	// the cells the pass sets hold change only with what comes of the frame (cells())
	m_active = work(Commands, Maps, Camera);
	return m_active;
}

bool snow_drifts::work(command_stream *const Commands, sky_occlusion_maps const &Maps, glm::dvec3 const &Camera)
{
	if ((Commands == nullptr) || (false == wanted(Maps)) || (false == ready()))
	{
		return false;
	}
	auto *const surface{Maps.surface()};
	auto const side{Maps.side()};
	if ((side <= 0) || ((side != m_side) && (false == allocate(side))))
	{
		return false;
	}
	// the texel of the camera in the toroidal maps, as gfx::sky_occlusion::setup_scene() places it
	auto const texel{static_cast<double>(Maps.map_texel())};
	auto const size{static_cast<double>(Maps.map_size())};
	glm::ivec2 const camera{static_cast<int>(std::floor(wrap(Camera.x, size) / texel)), static_cast<int>(std::floor(wrap(Camera.z, size) / texel))};
	auto const reach{Maps.map_reach()};
	auto const surfaceread{image_descriptor(m_nearest, surface->view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

	// the passes of the frames before read the cells, the kernels write them
	memory_barrier(Commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, compute_stage,
	               VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
	m_host.bind(m_drift);

	// the depths worked by the wind
	drift_constants drift{};
	drift.map = glm::ivec4{side, camera.x, camera.y, 0};
	drift.geometry = glm::vec4{static_cast<float>(texel), reach * 0.6f, reach * 0.3f, 0.f};
	barriers(Commands, {use_of(*m_drifted, VK_IMAGE_LAYOUT_GENERAL, compute_stage, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)});
	m_drift.dispatch(Commands, {surfaceread, storage_image(m_drifted->view())}, drift, groups(side), groups(side));
	barriers(Commands, {use_of(*m_drifted, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, compute_stage, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)});

	// smoothed into the cells
	m_host.bind(m_settle);
	settle_constants settle{};
	settle.map = glm::ivec4{side, 0, 0, 0};
	m_settle.dispatch(Commands, {surfaceread, image_descriptor(m_nearest, m_drifted->view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), buffer_descriptor(m_cells->handle(), 0, VK_WHOLE_SIZE)}, settle,
	                  groups(side), groups(side));
	compute_barrier(Commands);
	// the grid of the surface covers the filled part of the maps
	m_across = std::min(side, static_cast<int>(2.f * reach / Maps.map_texel()) + 1);
	return true;
}

VkBuffer snow_drifts::cells() const
{
	return (m_active && m_cells) ? m_cells->handle() : m_standin.handle();
}

bool snow_drifts::drawn() const
{
	return m_active && (Global.gfx_snow >= 2) && (m_across > 1) && (simulation::Environment.weather().snow().depth() > 0.02f);
}

std::string snow_drifts::info() const
{
	auto const &snow{simulation::Environment.weather().snow()};
	return std::format("snow: {:.2f} m, drifts {:.0f}%, loose {:.0f}%, wet {:.0f}%, blowing {:.0f}%, wire ice {:.1f} mm{}", snow.depth(), snow.drift() * 100.f, snow.fresh() * 100.f, snow.wetness() * 100.f,
	                   snow.blowing() * 100.f, snow.wire_ice() * 1000.f, m_active ? "" : " (no drifts)");
}

} // namespace vk14
