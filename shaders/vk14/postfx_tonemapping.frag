#version 460 core
#define STAGE_FRAGMENT 1
#define LAYOUT_POST 1
#include "common.glsl"
layout(location = 14) in vec2 f_coords;

layout(location = 0) out vec4 out_color;

#pragma texture(tex1, 0, RGB)
layout(set = POST_SET, binding = POST_TEXTURE_FIRST + 0) uniform sampler2D tex1;

// glow of the bright parts of the image (postfx_bloom_*.frag)
#pragma texture(bloom_tex, 1, RGB)
layout(set = POST_SET, binding = POST_TEXTURE_FIRST + 1) uniform sampler2D bloom_tex;


#include "tonemapping.glsl"
#include "dither.glsl"

// param is pc.data of the push constants (the model block in the opengl renderer).
// param[0].x: share of the bloom in the image (0: no bloom, the texture is not read), param[0].y: 1 if the automatic
// exposure is applied (exposure_ssbo of common.glsl, worked out by exposure_average.comp), param[0].z: fixed exposure multiplier, param[0].w: scale of
// the bloom texture (it holds the sum of the levels of the chain), param[1].x: 1 when this is the last pass before the
// window, which takes the dither, param[1].yzw: gains of the white balance (0: none),
// param[2]: the grade after the tonemapping (saturation, vibrance, contrast) and the sharpening before it (gfx.postfx.grade*)
void main()
{
	vec2 texcoord = f_coords;
	vec3 hdr_color = texture(tex1, texcoord).xyz;
	if (param[2].w > 0.0)
	{
		// sharpened against the blur of the temporal antialiasing: the difference from the cross of the neighbours added,
		// kept within their range so the edges get no halos
		vec3 n = textureOffset(tex1, texcoord, ivec2(0, 1)).xyz;
		vec3 s = textureOffset(tex1, texcoord, ivec2(0, -1)).xyz;
		vec3 e = textureOffset(tex1, texcoord, ivec2(1, 0)).xyz;
		vec3 w = textureOffset(tex1, texcoord, ivec2(-1, 0)).xyz;
		vec3 low = min(hdr_color, min(min(n, s), min(e, w)));
		vec3 high = max(hdr_color, max(max(n, s), max(e, w)));
		vec3 sharp = hdr_color + (hdr_color - (n + s + e + w) * 0.25) * param[2].w * 1.5;
		hdr_color = clamp(sharp, low, high);
	}
	if (param[0].x > 0.0)
		hdr_color = mix(hdr_color, texture(bloom_tex, texcoord).rgb * param[0].w, param[0].x);
	if (param[1].y > 0.0)
		hdr_color = white_balance(hdr_color, param[1].yzw);
	float exposurescale = param[0].z;
	if (param[0].y > 0.5 && exposure > 0.0)
		exposurescale *= exposure;
	out_color = tonemap_graded(vec4(hdr_color * exposurescale, 1.0), param[2].xyz);
	if (param[1].x > 0.5)
		out_color.rgb = dither_output(out_color.rgb);
}
