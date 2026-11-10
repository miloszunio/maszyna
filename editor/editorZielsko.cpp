/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorZielsko.hpp"
#include "editor/editorFormat.hpp"
#include "editor/editorHttp.hpp"
#include "stb/stb_image.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string_view>

namespace zielsko
{

namespace
{

char const *const wms{"https://mapy.geoportal.gov.pl/wss/service/PZGIK/FOTO/WMS/ModeleDrzew3D"};
char const *const layer{"Modele_3D_drzew_LoD1_2023"};
std::uint32_t constexpr cache_magic{0x4b534c5a}; // ZLSK
std::uint32_t constexpr cache_version{1};

std::uint32_t read32(std::uint8_t const *P)
{
	return static_cast<std::uint32_t>(P[0]) | (static_cast<std::uint32_t>(P[1]) << 8) | (static_cast<std::uint32_t>(P[2]) << 16) | (static_cast<std::uint32_t>(P[3]) << 24);
}

std::uint16_t read16(std::uint8_t const *P)
{
	return static_cast<std::uint16_t>(P[0] | (P[1] << 8));
}

std::uint64_t read64(std::uint8_t const *P)
{
	return static_cast<std::uint64_t>(read32(P)) | (static_cast<std::uint64_t>(read32(P + 4)) << 32);
}

// the metre of the plan a tree stands on, to tell the trunk and the crown of one tree, and a tree on two sheets, as one
std::uint64_t metre_of(double const East, double const North)
{
	auto const e{static_cast<std::int64_t>(std::llround(East))};
	auto const n{static_cast<std::int64_t>(std::llround(North))};
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(e)) << 32) | static_cast<std::uint32_t>(n);
}

// a zip read as it comes: the entries are taken one after another by their local headers, the sizes of which the packages give
// (no data descriptors), each handed over whole
class zip_stream
{
  public:
	using entry_sink = std::function<void(std::string const &Name, std::uint16_t Method, std::uint8_t const *Data, std::size_t Size)>;
	explicit zip_stream(entry_sink Sink) : m_sink(std::move(Sink)) {}

	// false once the zip can't be read on
	bool feed(std::uint8_t const *Data, std::size_t Size)
	{
		if (m_finished)
			return true;
		if (m_offset > 0 && m_offset >= m_pending.size() / 2)
		{
			m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<std::ptrdiff_t>(m_offset));
			m_offset = 0;
		}
		m_pending.insert(m_pending.end(), Data, Data + Size);
		for (;;)
		{
			auto const available{m_pending.size() - m_offset};
			if (available < 4)
				return true;
			auto const *head{m_pending.data() + m_offset};
			auto const signature{read32(head)};
			if (signature == 0x02014b50 || signature == 0x06054b50 || signature == 0x06064b50)
			{
				// the central directory: all the entries passed
				m_finished = true;
				return true;
			}
			if (signature != 0x04034b50)
			{
				m_error = "not a zip, or a zip of an unknown layout";
				return false;
			}
			if (available < 30)
				return true;
			auto const flags{read16(head + 6)};
			auto const method{read16(head + 8)};
			std::uint64_t compressed{read32(head + 18)};
			auto const namelength{read16(head + 26)};
			auto const extralength{read16(head + 28)};
			if (available < 30u + namelength + extralength)
				return true;
			if ((flags & 0x8) != 0)
			{
				m_error = "the zip keeps the sizes after the data, it can't be read as it comes";
				return false;
			}
			if (compressed == 0xffffffffu)
			{
				// zip64: the sizes in the extra field
				auto const *extra{head + 30 + namelength};
				for (std::size_t at = 0; at + 4 <= extralength;)
				{
					auto const id{read16(extra + at)};
					auto const size{read16(extra + at + 2)};
					if (id == 0x0001 && size >= 16 && at + 4 + 16 <= extralength)
						compressed = read64(extra + at + 4 + 8);
					at += 4u + size;
				}
			}
			auto const whole{30u + namelength + extralength + compressed};
			if (available < whole)
				return true;
			std::string const name(reinterpret_cast<char const *>(head + 30), namelength);
			m_sink(name, method, head + 30 + namelength + extralength, static_cast<std::size_t>(compressed));
			m_offset += static_cast<std::size_t>(whole);
		}
	}
	std::string const &error() const { return m_error; }

  private:
	entry_sink m_sink;
	std::vector<std::uint8_t> m_pending;
	std::size_t m_offset{0};
	bool m_finished{false};
	std::string m_error;
};

std::string_view next_tag(std::string_view const Text, std::size_t &At, std::string_view const Tag)
{
	auto const found{Text.find(Tag, At)};
	if (found == std::string_view::npos)
	{
		At = Text.size();
		return {};
	}
	At = found;
	return Text.substr(found);
}

} // namespace

std::vector<std::pair<double, double>> area::sample(double const Step) const
{
	std::set<std::pair<std::int64_t, std::int64_t>> taken;
	std::vector<std::pair<double, double>> result;
	for (auto const key : m_cells)
	{
		auto const e{static_cast<std::int32_t>(key >> 32)};
		auto const n{static_cast<std::int32_t>(key & 0xffffffffu)};
		auto const east{(e + 0.5) * cell};
		auto const north{(n + 0.5) * cell};
		auto const coarse{std::make_pair(static_cast<std::int64_t>(std::floor(east / Step)), static_cast<std::int64_t>(std::floor(north / Step)))};
		if (taken.insert(coarse).second)
			result.emplace_back(east, north);
	}
	return result;
}

std::string county_query(double const East, double const North)
{
	// WMS 1.1.1: the box in EPSG:2180 as east, north
	auto const d{20.0};
	return format("%s?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetFeatureInfo&LAYERS=%s&QUERY_LAYERS=%s&SRS=EPSG:2180&BBOX=%.0f,%.0f,%.0f,%.0f"
	              "&WIDTH=41&HEIGHT=41&X=20&Y=20&INFO_FORMAT=text/html&STYLES=",
	              wms, layer, layer, East - d, North - d, East + d, North + d);
}

bool county_of_answer(std::string const &Answer, county &Out)
{
	// .../InneDane/Drzewa3D/LOD1/2023/02/0219.zip
	auto const at{Answer.find("Drzewa3D/LOD1/")};
	if (at == std::string::npos)
		return false;
	auto const start{Answer.rfind("http", at)};
	auto const end{Answer.find(".zip", at)};
	if (start == std::string::npos || end == std::string::npos)
		return false;
	Out.url = Answer.substr(start, end + 4 - start);
	auto const slash{Out.url.find_last_of('/')};
	Out.teryt = Out.url.substr(slash + 1, Out.url.size() - slash - 1 - 4);
	return false == Out.teryt.empty();
}

std::size_t read_sheet(char const *Text, std::size_t const Size, area const &Area, std::unordered_set<std::uint64_t> &Seen, std::vector<tree> &Out)
{
	std::string_view const text{Text, Size};
	std::string_view const open{"<veg:SolitaryVegetationObject"};
	std::string_view const close{"</veg:SolitaryVegetationObject>"};
	std::size_t added{0};
	std::size_t at{0};
	while (at < text.size())
	{
		auto const start{text.find(open, at)};
		if (start == std::string_view::npos)
			break;
		auto const end{text.find(close, start)};
		if (end == std::string_view::npos)
			break;
		at = end + close.size();
		auto const object{text.substr(start, end - start)};
		// a tree is two objects: the trunk (a prism of 10 sides, 12 faces) and the crown (128 faces) over it; the trunk tells where
		// it stands by the ring of its foot, the first face. The crown is passed over
		std::size_t faces{0};
		for (std::size_t p = object.find("<gml:Polygon"); p != std::string_view::npos && faces <= 20; p = object.find("<gml:Polygon", p + 12))
			++faces;
		if (faces == 0 || faces > 20)
			continue;
		auto const heightat{object.find("<veg:height")};
		if (heightat == std::string_view::npos)
			continue;
		auto const heightvalue{object.find('>', heightat)};
		auto const height{std::strtod(object.data() + heightvalue + 1, nullptr)};
		auto const listat{object.find("<gml:posList")};
		if (listat == std::string_view::npos)
			continue;
		auto const listvalue{object.find('>', listat)};
		auto const listend{object.find('<', listvalue)};
		if (listvalue == std::string_view::npos || listend == std::string_view::npos)
			continue;
		// the ring of the foot: east, north, height of each point (the packages give them in that order), the last as the first
		std::string const numbers{object.substr(listvalue + 1, listend - listvalue - 1)};
		char const *p{numbers.c_str()};
		char *next{nullptr};
		double sum_e{0.0}, sum_n{0.0};
		int count{0};
		double first_e{0.0}, first_n{0.0};
		for (int i = 0;; ++i)
		{
			auto const e{std::strtod(p, &next)};
			if (next == p)
				break;
			p = next;
			auto const n{std::strtod(p, &next)};
			if (next == p)
				break;
			p = next;
			std::strtod(p, &next);
			if (next == p)
				break;
			p = next;
			if (i == 0)
			{
				first_e = e;
				first_n = n;
			}
			else if (e == first_e && n == first_n)
				break;
			sum_e += e;
			sum_n += n;
			++count;
		}
		if (count < 3 || height <= 0.0)
			continue;
		auto const east{sum_e / count};
		auto const north{sum_n / count};
		if (false == Area.contains(east, north))
			continue;
		if (false == Seen.insert(metre_of(east, north)).second)
			continue;
		Out.push_back({east, north, static_cast<float>(height)});
		++added;
	}
	return added;
}

bool write_cache(std::string const &Path, std::vector<tree> const &Trees)
{
	std::filesystem::create_directories(std::filesystem::path{Path}.parent_path());
	auto const temporary{Path + ".tmp"};
	{
		std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
		if (false == out.good())
			return false;
		auto const put32 = [&](std::uint32_t const Value) { out.write(reinterpret_cast<char const *>(&Value), 4); };
		put32(cache_magic);
		put32(cache_version);
		put32(static_cast<std::uint32_t>(Trees.size()));
		for (auto const &t : Trees)
		{
			// decimetres of the plan, centimetres of the height
			std::int32_t const e{static_cast<std::int32_t>(std::lround(t.east * 10.0))};
			std::int32_t const n{static_cast<std::int32_t>(std::lround(t.north * 10.0))};
			std::uint16_t const h{static_cast<std::uint16_t>(std::clamp(std::lround(t.height * 100.0), 0l, 65535l))};
			out.write(reinterpret_cast<char const *>(&e), 4);
			out.write(reinterpret_cast<char const *>(&n), 4);
			out.write(reinterpret_cast<char const *>(&h), 2);
		}
		if (false == out.good())
			return false;
	}
	std::error_code error;
	std::filesystem::rename(temporary, Path, error);
	if (error)
	{
		std::filesystem::remove(Path, error);
		std::filesystem::rename(temporary, Path, error);
	}
	return false == static_cast<bool>(error);
}

bool read_cache(std::string const &Path, std::vector<tree> &Trees)
{
	std::ifstream in{Path, std::ios::binary};
	if (false == in.good())
		return false;
	std::uint32_t magic{0}, version{0}, count{0};
	in.read(reinterpret_cast<char *>(&magic), 4);
	in.read(reinterpret_cast<char *>(&version), 4);
	in.read(reinterpret_cast<char *>(&count), 4);
	if (false == in.good() || magic != cache_magic || version != cache_version)
		return false;
	std::vector<char> data(static_cast<std::size_t>(count) * 10);
	in.read(data.data(), static_cast<std::streamsize>(data.size()));
	if (static_cast<std::size_t>(in.gcount()) != data.size())
		return false;
	Trees.reserve(Trees.size() + count);
	for (std::uint32_t i = 0; i < count; ++i)
	{
		std::int32_t e, n;
		std::uint16_t h;
		std::memcpy(&e, data.data() + i * 10, 4);
		std::memcpy(&n, data.data() + i * 10 + 4, 4);
		std::memcpy(&h, data.data() + i * 10 + 8, 2);
		Trees.push_back({e / 10.0, n / 10.0, h / 100.0f});
	}
	return true;
}

void harvest::start(std::string Folder, area Area, std::vector<std::pair<double, double>> Probes, bool const Again)
{
	stop();
	m_stop = false;
	m_running = true;
	m_progress = 0.0;
	m_done = 0;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		m_counties.clear();
		m_status.clear();
	}
	m_thread = std::thread([this, Folder = std::move(Folder), Area = std::move(Area), Probes = std::move(Probes), Again]() mutable {
		run(std::move(Folder), std::move(Area), std::move(Probes), Again);
		m_running = false;
	});
}

void harvest::stop()
{
	m_stop = true;
	if (m_thread.joinable())
		m_thread.join();
	m_running = false;
}

std::string harvest::status() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_status;
}

std::vector<county> harvest::counties() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_counties;
}

void harvest::set_status(std::string Text)
{
	std::lock_guard<std::mutex> lock{m_mutex};
	m_status = std::move(Text);
}

void harvest::run(std::string Folder, area Area, std::vector<std::pair<double, double>> Probes, bool const Again)
{
	editor_http::client client;
	auto const stopped = [this]() { return m_stop.load(); };
	// the counties under the area: the WMS asked about a point every few kilometres of it
	std::map<std::string, county> found;
	for (std::size_t i = 0; i < Probes.size() && false == stopped(); ++i)
	{
		m_progress = static_cast<double>(i) / std::max<std::size_t>(1, Probes.size());
		set_status(format(STR_C("Asking which counties lie under the scenery: %zu of %zu points, %zu counties"), i, Probes.size(), found.size()));
		std::vector<std::uint8_t> answer;
		std::string error;
		if (false == client.get(county_query(Probes[i].first, Probes[i].second), answer, error, stopped))
			continue;
		county c;
		if (county_of_answer(std::string(answer.begin(), answer.end()), c))
			found.emplace(c.teryt, c);
	}
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		for (auto const &[teryt, c] : found)
			m_counties.push_back(c);
	}
	if (stopped())
	{
		set_status(STR_C("Stopped"));
		return;
	}
	WriteLog("ZIELSKO: " + std::to_string(found.size()) + " counties under the scenery");
	std::size_t index{0};
	std::size_t total_trees{0};
	for (auto const &[teryt, c] : found)
	{
		++index;
		if (stopped())
			break;
		auto const path{Folder + teryt + ".drz"};
		if (false == Again && std::filesystem::exists(path))
		{
			++m_done;
			continue;
		}
		std::vector<tree> trees;
		std::unordered_set<std::uint64_t> seen;
		std::size_t sheets{0};
		std::uint64_t length{0}, received{0};
		zip_stream zip{[&](std::string const &Name, std::uint16_t const Method, std::uint8_t const *Data, std::size_t const Size) {
			if (Name.size() < 4 || Name.compare(Name.size() - 4, 4, ".gml") != 0)
				return;
			++sheets;
			if (Method == 0)
			{
				read_sheet(reinterpret_cast<char const *>(Data), Size, Area, seen, trees);
				return;
			}
			if (Method != 8 || Size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
				return;
			int length_out{0};
			auto *text{stbi_zlib_decode_noheader_malloc(reinterpret_cast<char const *>(Data), static_cast<int>(Size), &length_out)};
			if (text == nullptr)
			{
				WriteLog("ZIELSKO: can't unpack " + Name);
				return;
			}
			read_sheet(text, static_cast<std::size_t>(length_out), Area, seen, trees);
			std::free(text);
		}};
		std::string error;
		auto const ok{client.get_stream(
		    c.url,
		    [&](std::uint8_t const *Data, std::size_t const Size) {
			    received += Size;
			    auto const part{length > 0 ? static_cast<double>(received) / static_cast<double>(length) : 0.0};
			    m_progress = (static_cast<double>(index - 1) + part) / static_cast<double>(found.size());
			    set_status(format(STR_C("County %s (%zu of %zu): %.0f of %.0f MB read, %zu sheets, %zu trees kept"), teryt.c_str(), index, found.size(),
			                      received / 1048576.0, length / 1048576.0, sheets, trees.size()));
			    return zip.feed(Data, Size);
		    },
		    length, error, stopped)};
		if (false == ok)
		{
			if (false == zip.error().empty())
				error = zip.error();
			WriteLog("ZIELSKO: county " + teryt + " not read: " + error);
			if (stopped())
				break;
			continue;
		}
		if (false == write_cache(path, trees))
		{
			WriteLog("ZIELSKO: can't write " + path);
			continue;
		}
		total_trees += trees.size();
		++m_done;
		WriteLog("ZIELSKO: county " + teryt + ": " + std::to_string(sheets) + " sheets, " + std::to_string(trees.size()) + " trees kept");
	}
	m_progress = 1.0;
	set_status(stopped() ? std::string{STR_C("Stopped")}
	                     : format(STR_C("Done: %zu of %zu counties read, %zu trees kept now"), m_done.load(), found.size(), total_trees));
}

} // namespace zielsko
