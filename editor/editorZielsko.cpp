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
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace zielsko
{

namespace
{

char const *const wms{"https://mapy.geoportal.gov.pl/wss/service/PZGIK/FOTO/WMS/ModeleDrzew3D"};
char const *const layer{"Modele_3D_drzew_LoD1_2023"};
std::uint32_t constexpr cache_magic{0x4b534c5a}; // ZLSK
std::uint32_t constexpr cache_version{2};

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

// the sheet of the name of a file of a package: 0265/0265_M-33-45-A-d-4-4-4.gml
std::string sheet_of_file(std::string const &Name)
{
	auto start{Name.find_last_of('/')};
	start = start == std::string::npos ? 0 : start + 1;
	auto const underscore{Name.find('_', start)};
	if (underscore != std::string::npos)
		start = underscore + 1;
	auto const end{Name.rfind('.')};
	return Name.substr(start, end == std::string::npos || end < start ? std::string::npos : end - start);
}

} // namespace

void geographic(double const East, double const North, double &Latitude, double &Longitude)
{
	// the transverse Mercator of PUWG 1992 (GRS 80, meridian 19, scale 0.9993, 500 000 east, -5 300 000 north), inverted by the
	// series of Krueger
	constexpr double a{6378137.0};
	constexpr double f{1.0 / 298.257222101};
	constexpr double k0{0.9993};
	double const n{f / (2.0 - f)};
	double const A{a / (1.0 + n) * (1.0 + n * n / 4.0 + n * n * n * n / 64.0)};
	std::array<double, 3> const beta{n / 2.0 - 2.0 * n * n / 3.0 + 37.0 * n * n * n / 96.0, n * n / 48.0 + n * n * n / 15.0, 17.0 * n * n * n / 480.0};
	std::array<double, 3> const delta{2.0 * n - 2.0 * n * n / 3.0 - 2.0 * n * n * n, 7.0 * n * n / 3.0 - 8.0 * n * n * n / 5.0, 56.0 * n * n * n / 15.0};
	double const xi{(North + 5300000.0) / (k0 * A)};
	double const eta{(East - 500000.0) / (k0 * A)};
	double xi_{xi};
	double eta_{eta};
	for (int j = 1; j <= 3; ++j)
	{
		xi_ -= beta[j - 1] * std::sin(2.0 * j * xi) * std::cosh(2.0 * j * eta);
		eta_ -= beta[j - 1] * std::cos(2.0 * j * xi) * std::sinh(2.0 * j * eta);
	}
	double const chi{std::asin(std::sin(xi_) / std::cosh(eta_))};
	double latitude{chi};
	for (int j = 1; j <= 3; ++j)
		latitude += delta[j - 1] * std::sin(2.0 * j * chi);
	constexpr double degrees{180.0 / 3.14159265358979323846};
	Latitude = latitude * degrees;
	Longitude = 19.0 + std::atan2(std::sinh(eta_), std::cos(xi_)) * degrees;
}

std::string sheet_of(double const East, double const North)
{
	double latitude, longitude;
	geographic(East, North, latitude, longitude);
	// the sheet of 1:1 000 000 (4 x 6 degrees), of 1:100 000 (12 x 12 in it), then halved five times: A-D, a-d, 1-4, 1-4, 1-4
	auto const row{static_cast<int>(std::floor(latitude / 4.0))};
	auto const zone{static_cast<int>(std::floor((longitude + 180.0) / 6.0)) + 1};
	std::string result{static_cast<char>('A' + row)};
	result += '-' + std::to_string(zone);
	double top{(row + 1) * 4.0};
	double left{(zone - 31) * 6.0};
	double height{4.0 / 12.0};
	double width{6.0 / 12.0};
	auto r{std::clamp(static_cast<int>(std::floor((top - latitude) / height)), 0, 11)};
	auto c{std::clamp(static_cast<int>(std::floor((longitude - left) / width)), 0, 11)};
	result += '-' + std::to_string(r * 12 + c + 1);
	top -= r * height;
	left += c * width;
	static char const *const names[]{"ABCD", "abcd", "1234", "1234", "1234"};
	for (auto const *quarter : names)
	{
		height /= 2.0;
		width /= 2.0;
		r = std::clamp(static_cast<int>(std::floor((top - latitude) / height)), 0, 1);
		c = std::clamp(static_cast<int>(std::floor((longitude - left) / width)), 0, 1);
		result += '-';
		result += quarter[r * 2 + c];
		top -= r * height;
		left += c * width;
	}
	return result;
}

std::uint64_t metre_of(double const East, double const North)
{
	auto const e{static_cast<std::int64_t>(std::llround(East))};
	auto const n{static_cast<std::int64_t>(std::llround(North))};
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(e)) << 32) | static_cast<std::uint32_t>(n);
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

std::size_t read_sheet(char const *Text, std::size_t const Size, std::unordered_set<std::uint64_t> &Seen, std::vector<tree> &Out)
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
		double sum_e{0.0};
		double sum_n{0.0};
		int count{0};
		double first_e{0.0};
		double first_n{0.0};
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
		if (false == Seen.insert(metre_of(east, north)).second)
			continue;
		Out.push_back({east, north, static_cast<float>(height)});
		++added;
	}
	return added;
}

bool write_cache(std::string const &Path, county_cache const &Cache)
{
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path{Path}.parent_path(), error);
	auto const temporary{Path + ".tmp"};
	{
		std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
		if (false == out.good())
			return false;
		auto const put32 = [&out](std::uint32_t const Value) { out.write(reinterpret_cast<char const *>(&Value), 4); };
		put32(cache_magic);
		put32(cache_version);
		put32((Cache.complete ? 1u : 0u) | (Cache.band ? 2u : 0u));
		put32(static_cast<std::uint32_t>(Cache.sheets.size()));
		for (auto const &sheet : Cache.sheets)
		{
			auto const length{static_cast<std::uint8_t>(std::min<std::size_t>(sheet.size(), 255))};
			out.put(static_cast<char>(length));
			out.write(sheet.data(), length);
		}
		put32(static_cast<std::uint32_t>(Cache.trees.size()));
		for (auto const &t : Cache.trees)
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
	std::filesystem::rename(temporary, Path, error);
	if (error)
	{
		std::filesystem::remove(Path, error);
		std::filesystem::rename(temporary, Path, error);
	}
	return false == static_cast<bool>(error);
}

bool read_cache(std::string const &Path, county_cache &Cache)
{
	Cache = {};
	std::ifstream in{Path, std::ios::binary};
	if (false == in.good())
		return false;
	auto const get32 = [&in]() {
		std::uint32_t value{0};
		in.read(reinterpret_cast<char *>(&value), 4);
		return value;
	};
	auto const magic{get32()};
	auto const version{get32()};
	if (false == in.good() || magic != cache_magic || (version != 1 && version != cache_version))
		return false;
	if (version == 1)
		// the first version: the trees near the tracks only, the sheets not known
		Cache.band = true;
	else
	{
		auto const flags{get32()};
		Cache.complete = (flags & 1u) != 0;
		Cache.band = (flags & 2u) != 0;
		auto const sheets{get32()};
		for (std::uint32_t i = 0; i < sheets && in.good(); ++i)
		{
			auto const length{static_cast<std::size_t>(static_cast<unsigned char>(in.get()))};
			std::string sheet(length, '\0');
			in.read(sheet.data(), static_cast<std::streamsize>(length));
			Cache.sheets.insert(std::move(sheet));
		}
	}
	auto const count{get32()};
	if (false == in.good())
		return false;
	std::vector<char> data(static_cast<std::size_t>(count) * 10);
	in.read(data.data(), static_cast<std::streamsize>(data.size()));
	if (static_cast<std::size_t>(in.gcount()) != data.size())
		return false;
	Cache.trees.reserve(count);
	for (std::uint32_t i = 0; i < count; ++i)
	{
		std::int32_t e;
		std::int32_t n;
		std::uint16_t h;
		std::memcpy(&e, data.data() + i * 10, 4);
		std::memcpy(&n, data.data() + i * 10 + 4, 4);
		std::memcpy(&h, data.data() + i * 10 + 8, 2);
		Cache.trees.push_back({e / 10.0, n / 10.0, h / 100.0f});
	}
	return true;
}

void harvest::start(std::string Folder, std::vector<probe> Probes, bool const Whole)
{
	stop();
	m_stop = false;
	m_finished = false;
	m_running = true;
	m_progress = 0.0;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		m_counties.clear();
		m_status.clear();
	}
	m_thread = std::thread([this, Folder = std::move(Folder), Probes = std::move(Probes), Whole]() mutable {
		run(std::move(Folder), std::move(Probes), Whole);
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

void harvest::run(std::string Folder, std::vector<probe> Probes, bool const Whole)
{
	editor_http::client client;
	auto const stopped = [this]() { return m_stop.load(); };
	// 1. the counties under the points, asked of the WMS, and the sheets each of them has to give. The answers are kept beside the
	// files of the counties (a line a point: east north teryt url), so a point asked once isn't asked again
	std::map<std::string, county> found;
	std::map<std::string, std::set<std::string>> wanted;
	std::map<std::pair<long long, long long>, county> answers;
	auto const answers_path{Folder + "counties.txt"};
	{
		std::ifstream in{answers_path};
		long long east, north;
		county c;
		while (in >> east >> north >> c.teryt >> c.url)
			answers[{east, north}] = c;
	}
	std::size_t asked{0};
	std::size_t failed{0};
	for (std::size_t i = 0; i < Probes.size() && false == stopped(); ++i)
	{
		m_progress = 0.05 * static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1, Probes.size()));
		std::pair<long long, long long> const key{std::llround(Probes[i].east), std::llround(Probes[i].north)};
		county c;
		if (auto const known{answers.find(key)}; known != answers.end())
			c = known->second;
		else
		{
			set_status(format(STR_C("Asking which counties lie under the area: %zu of %zu points, %zu counties"), i, Probes.size(), found.size()));
			std::vector<std::uint8_t> answer;
			std::string error;
			if (false == client.get(county_query(Probes[i].east, Probes[i].north), answer, error, stopped))
			{
				// not kept: asked again next time
				if (++failed == 1)
					WriteLog("ZIELSKO: the county under " + std::to_string(key.first) + " " + std::to_string(key.second) + " not asked: " + error);
				continue;
			}
			std::string const text(answer.begin(), answer.end());
			if (false == county_of_answer(text, c))
			{
				// an error of the service isn't an answer: asked again next time
				if (text.empty() || text.find("ServiceException") != std::string::npos)
				{
					if (++failed == 1)
						WriteLog("ZIELSKO: the service gave no answer for " + std::to_string(key.first) + " " + std::to_string(key.second) + ": " +
						         text.substr(0, 200));
					continue;
				}
				// no county of the models there (abroad): kept as such
				c = county{"-", "-"};
			}
			answers[key] = c;
			++asked;
		}
		if (c.teryt == "-")
			continue;
		found.emplace(c.teryt, c);
		wanted[c.teryt].insert(Probes[i].sheet);
	}
	if (asked > 0)
	{
		std::error_code error;
		std::filesystem::create_directories(Folder, error);
		std::ofstream out{answers_path, std::ios::trunc};
		for (auto const &[key, c] : answers)
			out << key.first << ' ' << key.second << ' ' << c.teryt << ' ' << c.url << '\n';
	}
	if (stopped())
	{
		set_status(STR_C("Stopped"));
		return;
	}
	// 2. the sheets read before taken away; the sizes of the packages still to be read
	std::map<std::string, county_cache> caches;
	for (auto &[teryt, c] : found)
	{
		auto &cache{caches[teryt]};
		read_cache(Folder + teryt + ".drz", cache);
		auto &sheets{wanted[teryt]};
		if (cache.complete)
			sheets.clear();
		else
			for (auto const &sheet : cache.sheets)
				sheets.erase(sheet);
		c.needed = Whole ? (cache.complete ? 0 : 1) : sheets.size();
		if (c.needed > 0)
		{
			// the length of the package: the transfer given up at its first bytes
			std::uint64_t length{0};
			std::string error;
			client.get_stream(c.url, [](std::uint8_t const *, std::size_t) { return false; }, length, error, stopped);
			c.size = length;
		}
	}
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		for (auto const &[teryt, c] : found)
			m_counties.push_back(c);
	}
	std::uint64_t total{0};
	std::size_t todo{0};
	for (auto const &[teryt, c] : found)
		if (c.needed > 0)
		{
			total += c.size;
			++todo;
		}
	if (failed > 0)
		WriteLog("ZIELSKO: " + std::to_string(failed) + " of " + std::to_string(Probes.size()) + " points not asked for their county (the network)");
	WriteLog("ZIELSKO: " + std::to_string(found.size()) + " counties under the area, " + std::to_string(todo) + " to be read, up to " +
	         std::to_string(total / 1048576) + " MB");
	// 3. the packages read until all the sheets wanted have come (or to the end), each sheet passed kept
	std::uint64_t before{0};
	std::size_t index{0};
	for (auto &[teryt, c] : found)
	{
		if (stopped())
			break;
		if (c.needed == 0)
			continue;
		++index;
		auto &cache{caches[teryt]};
		auto &sheets{wanted[teryt]};
		auto const path{Folder + teryt + ".drz"};
		std::unordered_set<std::uint64_t> seen;
		for (auto const &t : cache.trees)
			seen.insert(metre_of(t.east, t.north));
		std::size_t passed{0};
		std::size_t added{0};
		bool all_come{false};
		std::uint64_t length{0};
		std::uint64_t received{0};
		zip_stream zip{[&](std::string const &Name, std::uint16_t const Method, std::uint8_t const *Data, std::size_t const Size) {
			if (Name.size() < 4 || Name.compare(Name.size() - 4, 4, ".gml") != 0)
				return;
			++passed;
			auto const sheet{sheet_of_file(Name)};
			if (cache.sheets.count(sheet) != 0)
				return;
			if (Method == 0)
				added += read_sheet(reinterpret_cast<char const *>(Data), Size, seen, cache.trees);
			else if (Method == 8 && Size <= static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				int length_out{0};
				auto *text{stbi_zlib_decode_noheader_malloc(reinterpret_cast<char const *>(Data), static_cast<int>(Size), &length_out)};
				if (text == nullptr)
				{
					WriteLog("ZIELSKO: can't unpack " + Name);
					return;
				}
				added += read_sheet(text, static_cast<std::size_t>(length_out), seen, cache.trees);
				std::free(text);
			}
			else
				return;
			cache.sheets.insert(sheet);
			sheets.erase(sheet);
			if (false == Whole && sheets.empty())
				all_come = true;
		}};
		std::string error;
		auto const ok{client.get_stream(
		    c.url,
		    [&](std::uint8_t const *Data, std::size_t const Size) {
			    received += Size;
			    auto const part{static_cast<double>(before + received) / static_cast<double>(std::max<std::uint64_t>(1, total))};
			    m_progress = 0.05 + 0.95 * std::min(1.0, part);
			    if (Whole)
				    set_status(format(STR_C("County %s (%zu of %zu): %.0f of %.0f MB read, %zu sheets, %zu trees"), teryt.c_str(), index, todo,
				                      received / 1048576.0, length / 1048576.0, passed, cache.trees.size()));
			    else
				    set_status(format(STR_C("County %s (%zu of %zu): %.0f of up to %.0f MB read, %zu sheets passed, %zu wanted still to come"), teryt.c_str(), index,
				                      todo, received / 1048576.0, length / 1048576.0, passed, sheets.size()));
			    if (false == zip.feed(Data, Size))
				    return false;
			    // all the sheets wanted have come: the rest of the package isn't needed
			    return false == all_come;
		    },
		    length, error, stopped)};
		before += c.size;
		if (ok)
			cache.complete = true;
		else if (false == all_come)
		{
			if (false == zip.error().empty())
				error = zip.error();
			WriteLog("ZIELSKO: county " + teryt + " not read to the end: " + error);
		}
		if (cache.complete && cache.band)
			cache.band = false;
		// what's come is kept, also when stopped
		if (false == write_cache(path, cache))
			WriteLog("ZIELSKO: can't write " + path);
		WriteLog("ZIELSKO: county " + teryt + ": " + std::to_string(passed) + " sheets passed, " + std::to_string(added) + " trees added, " +
		         std::to_string(received / 1048576) + " MB read");
		{
			std::lock_guard<std::mutex> lock{m_mutex};
			for (auto &entry : m_counties)
				if (entry.teryt == teryt)
					entry.needed = sheets.size();
		}
	}
	m_progress = 1.0;
	if (stopped())
	{
		set_status(STR_C("Stopped; what came is kept"));
		return;
	}
	m_finished = true;
	set_status(format(STR_C("Done: %zu counties under the area, %zu read now"), found.size(), todo));
}

} // namespace zielsko
