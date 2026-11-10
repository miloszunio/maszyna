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
// package a county, a GML file in it for each sheet of the map (of about 1.1 x 1.2 km, named by the grid of the 1:1 000 000
// map: M-33-45-A-d-4-4-4). Each tree there is a trunk and a crown of one shape scaled by its height, so all it says is where
// the tree stands (to a metre) and how tall it is.
// The server gives a package only whole, from its start, so a sheet is read by reading the package up to it. The sheets
// needed are read as they come, the reading stopped once all of them have come, and every sheet passed on the way is kept,
// all its trees, in a small file of the county beside the terrain: a sheet read once is never fetched again

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
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

// what's kept of a county
struct county_cache
{
	std::set<std::string> sheets; // the sheets read whole
	bool complete{false}; // the whole package read: a sheet not among the read ones has no trees
	bool band{false}; // a file of the first version: only the trees near the tracks, no sheets known
	std::vector<tree> trees;
};

// a county package
struct county
{
	std::string teryt; // 0219
	std::string url;
	std::uint64_t size{0}; // bytes of the package, once known
	std::size_t needed{0}; // sheets of it still to be read
};

// latitude and longitude (degrees, GRS 80) of a point of PUWG 1992
void geographic(double East, double North, double &Latitude, double &Longitude);
// the sheet of the map a point of PUWG 1992 lies on, as the packages name their files
std::string sheet_of(double East, double North);

// the address of the question to the WMS of the tree models about the point, and the package it answers with
std::string county_query(double East, double North);
bool county_of_answer(std::string const &Answer, county &Out);

// all the trees of the GML of a sheet, each once (Seen: their metre of the plan); returns how many were added
std::size_t read_sheet(char const *Text, std::size_t Size, std::unordered_set<std::uint64_t> &Seen, std::vector<tree> &Out);
// the metre of the plan a tree stands on
std::uint64_t metre_of(double East, double North);

// the file of a county
bool write_cache(std::string const &Path, county_cache const &Cache);
bool read_cache(std::string const &Path, county_cache &Cache);

// a point to be covered: where it is and the sheet it lies on
struct probe
{
	double east{0.0};
	double north{0.0};
	std::string sheet;
};

// the counties under the points asked of the WMS, and the sheets of them not read yet read from their packages, in a thread of
// their own
class harvest
{
  public:
	~harvest() { stop(); }
	// Folder: where the files of the counties go; Probes: the points to be covered; Whole: the packages read to the end rather
	// than stopped once the sheets of the points have come
	void start(std::string Folder, std::vector<probe> Probes, bool Whole);
	void stop();
	bool running() const { return m_running; }
	// done (not stopped) since the last start
	bool finished() const { return m_finished; }
	// what it's doing, for the window
	std::string status() const;
	double progress() const { return m_progress; }
	std::vector<county> counties() const;

  private:
	void run(std::string Folder, std::vector<probe> Probes, bool Whole);
	void set_status(std::string Text);

	std::thread m_thread;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_stop{false};
	std::atomic<bool> m_finished{false};
	std::atomic<double> m_progress{0.0};
	mutable std::mutex m_mutex;
	std::string m_status;
	std::vector<county> m_counties;
};

} // namespace zielsko
