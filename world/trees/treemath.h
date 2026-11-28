/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the arithmetic of the tree generator, made so that a seed gives the very same tree on every compiler and every machine: the
// random numbers come from our own generator and our own distributions (those of <random> give other numbers with libstdc++ and
// with MSVC), and the angles go through our own sine and cosine (std::sin and std::cos may differ in the last bit between the
// libraries). the generator uses no other function than + - * / sqrt and floor, which IEEE 754 rounds the same everywhere; its
// files are built without contracting a*b+c into one instruction (CMakeLists.txt)

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

#include <glm/glm.hpp>

namespace trees
{

// a permuted congruential generator (PCG32, XSH RR): small, fast and the same on every platform
class random
{
  public:
	explicit random(std::uint64_t const Seed, std::uint64_t const Stream = 0xda3e39cb94b95bdbULL) : m_increment{(Stream << 1U) | 1U}
	{
		next();
		m_state += Seed;
		next();
	}
	std::uint32_t next()
	{
		auto const old{m_state};
		m_state = old * 6364136223846793005ULL + m_increment;
		auto const shifted{static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U)};
		auto const rotation{static_cast<std::uint32_t>(old >> 59U)};
		return (shifted >> rotation) | (shifted << ((32U - rotation) & 31U));
	}
	// [0, 1), 24 bits: exactly representable as a float
	float unit()
	{
		return static_cast<float>(next() >> 8U) * (1.f / 16777216.f);
	}
	// [Low, High)
	float range(float const Low, float const High)
	{
		// std::lerp is computed differently by the standard libraries, and the tree must not depend on it
		return Low + (High - Low) * unit(); // NOSONAR(cpp:S6179) see above
	}
	// [-Half, Half)
	float spread(float const Half)
	{
		return range(-Half, Half);
	}
	// Low..High, both included
	int integer(int const Low, int const High)
	{
		if (High <= Low)
			return Low;
		return Low + static_cast<int>(next() % static_cast<std::uint32_t>(High - Low + 1));
	}
	bool chance(float const Probability)
	{
		return unit() < Probability;
	}
	// a point in the ball of radius 1
	glm::vec3 in_ball()
	{
		for (int attempt = 0; attempt < 32; ++attempt)
		{
			glm::vec3 const point{range(-1.f, 1.f), range(-1.f, 1.f), range(-1.f, 1.f)};
			if (glm::dot(point, point) <= 1.f)
				return point;
		}
		return glm::vec3{0.f};
	}
	// a direction, any
	glm::vec3 direction()
	{
		for (int attempt = 0; attempt < 32; ++attempt)
		{
			glm::vec3 const point{range(-1.f, 1.f), range(-1.f, 1.f), range(-1.f, 1.f)};
			if (auto const length2{glm::dot(point, point)}; length2 > 1e-4f && length2 <= 1.f)
				return point / std::sqrt(length2);
		}
		return glm::vec3{0.f, 1.f, 0.f};
	}
	// a generator of its own for a part of the work: what one part draws doesn't shift the numbers of another
	random fork(std::uint32_t const Salt)
	{
		auto const high{static_cast<std::uint64_t>(next())};
		auto const low{static_cast<std::uint64_t>(next())};
		return random{(high << 32U) | low, 0x5851f42d4c957f2dULL + Salt};
	}

  private:
	std::uint64_t m_state{0};
	std::uint64_t m_increment;
};

// FNV-1a of a text, to mix the name of a species into the seed
constexpr std::uint64_t hash_text(std::string_view const Text)
{
	std::uint64_t hash{0xcbf29ce484222325ULL};
	for (auto const character : Text)
	{
		hash ^= static_cast<std::uint8_t>(character);
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

inline float sine(float const Angle)
{
	constexpr float two_pi{2.f * std::numbers::pi_v<float>};
	constexpr float pi{std::numbers::pi_v<float>};
	constexpr float half_pi{0.5f * std::numbers::pi_v<float>};
	// into -pi..pi, then into -pi/2..pi/2 by sin(x) = sin(pi - x)
	auto x{Angle - two_pi * std::floor(Angle / two_pi + 0.5f)};
	if (x > half_pi)
		x = pi - x;
	if (x < -half_pi)
		x = -pi - x;
	// Taylor to the 11th power: under 1e-7 off on -pi/2..pi/2
	auto const x2{x * x};
	return x * (1.f + x2 * (-1.f / 6.f + x2 * (1.f / 120.f + x2 * (-1.f / 5040.f + x2 * (1.f / 362880.f - x2 / 39916800.f)))));
}

inline float cosine(float const Angle)
{
	return sine(Angle + 0.5f * std::numbers::pi_v<float>);
}

inline float radians(float const Degrees)
{
	return Degrees * (std::numbers::pi_v<float> / 180.f);
}

// a unit vector, or Fallback for one too short to have a direction
inline glm::vec3 unit(glm::vec3 const &Vector, glm::vec3 const &Fallback = glm::vec3{0.f, 1.f, 0.f})
{
	auto const length2{glm::dot(Vector, Vector)};
	if (length2 < 1e-12f)
		return Fallback;
	return Vector / std::sqrt(length2);
}

// any unit vector square to Direction
inline glm::vec3 square_to(glm::vec3 const &Direction)
{
	auto const helper{std::abs(Direction.y) < 0.9f ? glm::vec3{0.f, 1.f, 0.f} : glm::vec3{1.f, 0.f, 0.f}};
	return unit(glm::cross(Direction, helper));
}

// Vector turned by Angle about the unit Axis (Rodrigues)
inline glm::vec3 turned(glm::vec3 const &Vector, glm::vec3 const &Axis, float const Angle)
{
	auto const c{cosine(Angle)};
	auto const s{sine(Angle)};
	return Vector * c + glm::cross(Axis, Vector) * s + Axis * (glm::dot(Axis, Vector) * (1.f - c));
}

} // namespace trees
