/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// a minimal blocking https GET for the services the editor fetches its aids from (orthophoto, terrain model)
namespace editor_http
{

// true to give the transfer up
using stop_check = std::function<bool()>;

// one instance per worker thread
class client
{
  public:
	client();
	~client();
	client(client const &) = delete;
	client &operator=(client const &) = delete;

	bool get(std::string const &Url, std::vector<std::uint8_t> &Out, std::string &Error, stop_check const &Stop);
	// the same for a body too big to be kept whole: its bytes handed to Sink as they come (false from it gives the transfer up),
	// Length set to the length the server gives (0 if it gives none) before the first of them
	using sink = std::function<bool(std::uint8_t const *Data, std::size_t Size)>;
	bool get_stream(std::string const &Url, sink const &Sink, std::uint64_t &Length, std::string &Error, stop_check const &Stop);

  private:
	struct platform;
	std::unique_ptr<platform> m_platform;
};

// whether this build can fetch anything
bool available();

} // namespace editor_http
