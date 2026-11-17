// the light of the canopy of a crop (grass.frag of opengl46 and vulkan14, the same file in shaders/ogl46 and shaders/vk14), after
// light_common.glsl: f_canopy of grass_plant.glsl tells how the light of the sun gets into the canopy, through it and off it.
// the blades of grass have it all 0, and are lit as they always were

layout(location = 6) in vec4 f_canopy;

// the colour as lit: the deeper in the canopy, the less of the sun gets down there, the less so the higher the sun stands
vec3 canopy_albedo(vec3 albedo)
{
	if ((f_canopy.w <= 0.0) || (lights_count == 0U))
		return albedo;
	vec3 sun = -normalize(mat3(inv_view) * lights[0].dir);
	float through = f_canopy.z / max(sun.y, 0.15);
	return albedo * mix(1.0, exp(-0.35 * f_canopy.w * through), 0.7);
}

// the light added to what apply_lights() gives: the sun through the leaves and the ears seen against it, and the sheen of the wax
// and the awns at grazing angles
vec3 canopy_light(vec3 fragnormal, vec3 albedo)
{
	if (((f_canopy.x <= 0.0) && (f_canopy.y <= 0.0)) || (lights_count == 0U))
		return vec3(0.0);
	vec3 L = normalize(-lights[0].dir);
	vec3 V = normalize(-f_pos.xyz);
	float sunlit = (1.0 - clamp(calc_shadow(), 0.0, 1.0)) * cloud_shadow(mat3(inv_view) * f_pos.xyz);
	vec3 sunlight = lights[0].color * (SUN_DIFFUSE_SCALE * lights[0].intensity * sunlit);
	float top = 1.0 - f_canopy.z;
	float against = pow(max(dot(-V, L), 0.0), 4.0);
	vec3 through = albedo * vec3(1.05, 1.15, 0.7) * (f_canopy.x * against * (0.3 + 0.7 * top));
	float grazing = pow(1.0 - max(dot(fragnormal, V), 0.0), 4.0);
	vec3 sheen = vec3(0.85, 0.9, 1.0) * (f_canopy.y * grazing * top * 0.25);
	return sunlight * through + (sunlight * max(dot(fragnormal, L), 0.0) + ambient) * sheen;
}
