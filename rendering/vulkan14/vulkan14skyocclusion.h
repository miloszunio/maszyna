/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// the maps of the occlusion of the sky by the scenery in the vulkan renderer (the track T, opengl46_sky_occlusion): the images of
// gfx::sky_occlusion, which works out what goes into them. the squares of the tiles it gives are gathered in a frame and copied at
// once from one staging buffer, with the commands of the frame (flush()). with the snow drawn (Global.gfx_snow) it keeps the surface
// of the scenery the drifts are worked out from as well

#pragma once

#include "rendering/common/skyocclusion.h"
#include "rendering/vulkan14/vulkan14memory.h"

#include <memory>

namespace vk14
{

class command_stream;

class sky_occlusion_maps : public gfx::sky_occlusion
{
  public:
	sky_occlusion_maps() = default;
	~sky_occlusion_maps() override;

	// Memory, Deletions: what the maps are made and let go with; called once
	void init(allocator &Memory, deletion_queue &Deletions);
	// the maps go (the device is about to)
	void destroy();
	// the value of the timeline the gpu reaches when it's done with the frame being recorded
	void begin_frame(std::uint64_t const Value)
	{
		m_value = Value;
	}
	// the squares given in the frame copied to the maps, which are left in the layout the shading reads them in
	void flush(command_stream *Commands);
	// the maps once they're filled, for the pass sets; null: the stand-ins are read
	image *heights() const
	{
		return m_ready ? m_heights.get() : nullptr;
	}
	image *occlusion() const
	{
		return m_ready ? m_occlusion.get() : nullptr;
	}
	// the surface of the scenery for the snow (four 32 bit floats per texel, gfx::sky_occlusion::gpu_upload_surface()) once it's
	// filled, read texel by texel by the kernels of the drifts (vk14::snow_drifts); null without it (no snow drawn, no maps)
	image *surface() const
	{
		return m_surfaceready ? m_surface.get() : nullptr;
	}
	// texels across the maps
	int side() const
	{
		return m_side;
	}

  protected:
	bool gpu_allocate(int const Side) override;
	void gpu_release() override;
	void gpu_upload(int const X, int const Y, int const Size, float const *Heights, std::uint16_t const *Occlusion) override;
	void gpu_upload_surface(int const X, int const Y, int const Size, float const *Surface) override;

  private:
	void let_go();

	allocator *m_memory{nullptr};
	deletion_queue *m_deletions{nullptr};
	std::uint64_t m_value{0};
	std::shared_ptr<image> m_heights;
	std::shared_ptr<image> m_occlusion;
	std::shared_ptr<image> m_surface;
	bool m_ready{false};
	bool m_surfaceready{false};
	// the squares waiting for the copy: where, and their data one after another (the heights, then the occlusion of each)
	struct square
	{
		int x;
		int y;
		int size;
		std::size_t offset; // of the heights in m_data
	};
	std::vector<square> m_squares;
	// those of the surface for the snow, their data in m_data as well
	std::vector<square> m_surfacesquares;
	std::vector<std::uint8_t> m_data;
};

} // namespace vk14
