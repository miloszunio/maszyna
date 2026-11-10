/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// ZIELSKO z kosmosu (ficzery2): the trees of the 3D tree models of geoportal.gov.pl (GUGiK, CityGML LoD1, CC BY 4.0), one
// package a county. Each tree there is a trunk and a crown of one shape scaled by its height, so all it says is where the
// tree stands (to a metre) and how tall it is. The packages are read as they come over the network, a sheet of the map at a
// time, and only the trees over the terrain near the tracks are kept, in a small file of the county beside the terrain

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace zielsko
{

// a tree in PUWG 1992 (EPSG:2180)
struct tree
{
	double east{0.0};
	double north{0.0};
	float height{0.0f}; // metres
};

// a county package
struct county
{
	std::string teryt; // 0219
	std::string url;
};

// the cells of 50 m in PUWG 1992 where the trees are kept
class area
{
  public:
	static constexpr double cell{50.0};
	static std::uint64_t key(double const East, double const North)
	{
		auto const e{static_cast<std::int64_t>(std::floor(East / cell))};
		auto const n{static_cast<std::int64_t>(std::floor(North / cell))};
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(e)) << 32) | static_cast<std::uint32_t>(n);
	}
	void add(double const East, double const North) { m_cells.insert(key(East, North)); }
	bool contains(double const East, double const North) const { return m_cells.count(key(East, North)) != 0; }
	std::size_t size() const { return m_cells.size(); }
	bool empty() const { return m_cells.empty(); }
	// the centres of the cells, every Step metres at most one
	std::vector<std::pair<double, double>> sample(double Step) const;

  private:
	std::unordered_set<std::uint64_t> m_cells;
};

// the address of the question to the WMS of the tree models about the point, and the package it answers with
std::string county_query(double East, double North);
bool county_of_answer(std::string const &Answer, county &Out);

// the trees of the GML of a sheet within Area, each once (Seen: their metre of the plan); returns how many were added
std::size_t read_sheet(char const *Text, std::size_t Size, area const &Area, std::unordered_set<std::uint64_t> &Seen, std::vector<tree> &Out);

// the file of the trees kept of a county
bool write_cache(std::string const &Path, std::vector<tree> const &Trees);
bool read_cache(std::string const &Path, std::vector<tree> &Trees);

// the counties asked for and the packages read, in a thread of their own
class harvest
{
  public:
	~harvest() { stop(); }
	// Folder: where the files of the counties go; Area: what's kept; Counties: the packages to read, those with a file
	// already skipped unless Again
	void start(std::string Folder, area Area, std::vector<std::pair<double, double>> Probes, bool Again);
	void stop();
	bool running() const { return m_running; }
	// what it's doing, for the window
	std::string status() const;
	double progress() const { return m_progress; }
	// the counties found, and the ones of them read (their files written) since the start
	std::vector<county> counties() const;
	std::size_t done() const { return m_done; }

  private:
	void run(std::string Folder, area Area, std::vector<std::pair<double, double>> Probes, bool Again);
	void set_status(std::string Text);

	std::thread m_thread;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_stop{false};
	std::atomic<double> m_progress{0.0};
	std::atomic<std::size_t> m_done{0};
	mutable std::mutex m_mutex;
	std::string m_status;
	std::vector<county> m_counties;
};

} // namespace zielsko
