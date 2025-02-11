/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          https://www.mrpt.org/                         |
   |                                                                        |
   | Copyright (c) 2005-2022, Individual contributors, see AUTHORS file     |
   | See: https://www.mrpt.org/Authors - All rights reserved.               |
   | Released under BSD License. See: https://www.mrpt.org/License          |
   +------------------------------------------------------------------------+ */

#include "comms-precomp.h"	// Precompiled headers
//
#include <mrpt/comms/CClientTCPSocket.h>
#include <mrpt/comms/CServerTCPSocket.h>
#include <mrpt/comms/net_utils.h>
#include <mrpt/core/exceptions.h>
#include <mrpt/core/format.h>
#include <mrpt/system/CTicTac.h>
#include <mrpt/system/string_utils.h>

#include <cstdio>
#include <cstring>
#include <future>
#include <thread>

#if defined(MRPT_OS_LINUX) || defined(__APPLE__)
#define INVALID_SOCKET (-1)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#endif

#ifdef _WIN32
#include <winsock.h>

#include <iostream>
#endif

using namespace mrpt;
using namespace mrpt::system;
using namespace mrpt::comms;
using namespace mrpt::comms::net;
using namespace std;

/*---------------------------------------------------------------
							http_get
  ---------------------------------------------------------------*/
http_errorcode mrpt::comms::net::http_get(
	const string& url, string& out_content, const HttpRequestOptions& options,
	mrpt::optional_ref<HttpRequestOutput> output)
{
	std::vector<uint8_t> data;
	http_errorcode ret = http_get(url, data, options, output);

	out_content.resize(data.size());
	if (!data.empty()) std::memcpy(&out_content[0], &data[0], data.size());

	return ret;
}

http_errorcode mrpt::comms::net::http_request(
	const string& http_method, const string& http_send_content,
	const string& url, std::vector<uint8_t>& out_content,
	const HttpRequestOptions& options,
	mrpt::optional_ref<HttpRequestOutput> output)
{
	// Reset output data:
	out_content.clear();
	if (output) output.value().get() = HttpRequestOutput();

	// URL must be:
	// http://<SERVER>/<LOCAL_ADDR>

	if (0 != ::strncmp(url.c_str(), "http://", 7))
	{
		if (output)
			output.value().get().errormsg = "URL must start with 'http://'";
		return net::http_errorcode::BadURL;
	}

	string server_addr = url.substr(7);
	string get_object = "/";

	// Remove from the first "/" on:
	size_t pos = server_addr.find("/");
	if (pos == 0)
	{
		if (output)
			output.value().get().errormsg = "Server name not found in URL";
		return http_errorcode::BadURL;
	}
	if (pos != string::npos)
	{
		get_object = server_addr.substr(pos);
		server_addr.resize(pos);
	}

	CClientTCPSocket sock;

	try
	{
		// Connect:
		sock.connect(server_addr, options.port, options.timeout_ms);
	}
	catch (const std::exception& e)
	{
		if (output) output.value().get().errormsg = e.what();
		return net::http_errorcode::CouldntConnect;
	}

	try
	{
		// Set the user-defined headers (we may overwrite them if needed)
		std::map<std::string, std::string> headers_to_send =
			options.extra_headers;

		headers_to_send["Connection"] = "close";  // Don't keep alive

		if (!headers_to_send.count("User-Agent"))
			headers_to_send["User-Agent"] = "MRPT Library";

		// Implement HTTP Basic authentication:
		// See: http://en.wikipedia.org/wiki/Basic_access_authentication
		if (!options.auth_user.empty())
		{
			string auth_str =
				options.auth_user + string(":") + options.auth_pass;
			std::vector<uint8_t> v(auth_str.size());
			std::memcpy(&v[0], &auth_str[0], auth_str.size());

			string encoded_str;
			mrpt::system::encodeBase64(v, encoded_str);

			headers_to_send["Authorization"] = string("Basic ") + encoded_str;
		}

		if (!http_send_content.empty() &&
			headers_to_send.find("Content-Length") == headers_to_send.end())
		{
			headers_to_send["Content-Length"] =
				mrpt::format("%u", (unsigned int)http_send_content.size());
		}

		// Prepare the request string
		// ---------------------------------
		string req = format(
			"%s %s HTTP/1.1\r\n"
			"Host: %s\r\n",
			http_method.c_str(), get_object.c_str(), server_addr.c_str());

		// Other headers:
		for (auto i = headers_to_send.begin(); i != headers_to_send.end(); ++i)
		{
			req += i->first;
			req += ": ";
			req += i->second;
			req += "\r\n";
		}

		// End:
		req += "\r\n";

		// Any POST data?
		req += http_send_content;

		// Send:
		sock.sendString(req);

		// Read answer:
		std::vector<uint8_t> buf;
		buf.reserve(1 << 14);

		size_t total_read = 0;
		bool content_length_read = false;
		bool all_headers_read = false;
		size_t content_length = 0;
		size_t content_offset = 0;
		int http_code = 0;

		std::map<std::string, std::string> rx_headers;

		CTicTac watchdog;
		watchdog.Tic();

		while (!content_length_read ||
			   total_read < (content_offset + content_length))
		{
			// Read until "Content-Length: XXX \r\n" is read or the whole
			// message is read,
			//  or an error code is read.
			size_t to_read_now;

			if (!content_length_read) { to_read_now = 1500; }
			else
			{
				to_read_now = (content_length + content_offset) - total_read;
			}

			// make room for the data to come:
			buf.resize(total_read + to_read_now + 1);

			// Read:
			size_t len = sock.readAsync(
				&buf[total_read], to_read_now, options.timeout_ms, 100);
			if (!len)
			{
				//
				if (!sock.isConnected())
				{
					if (all_headers_read)  // It seems we're done...
						break;
					else
					{
						if (output)
							output.value().get().errormsg =
								"Connection to server was lost";
						return net::http_errorcode::CouldntConnect;
					}
				}

				if (watchdog.Tac() > 1e-3 * options.timeout_ms)
				{
					if (output)
						output.value().get().errormsg =
							"Timeout waiting answer from server";
					return net::http_errorcode::CouldntConnect;
				}
				std::this_thread::sleep_for(10ms);
				continue;
			}
			total_read += len;
			watchdog.Tic();

			buf[total_read] = '\0';

			// do we have a \r\n\r\n  ??
			if (!all_headers_read)
			{
				const char* ptr = ::strstr(
					reinterpret_cast<const char*>(&buf[0]), "\r\n\r\n");
				if (ptr)
				{
					all_headers_read = true;
					const size_t pos_dblret = ((char*)ptr) - (char*)(&buf[0]);

					// Process the headers:
					// ------------------------------
					if (!::strncmp("HTTP/", (const char*)&buf[0], 5))
					{ http_code = ::atoi((const char*)&buf[9]); }
					else
					{
						// May it be a "SOURCETABLE " answer for NTRIP
						// protocol??
						if (!::strncmp(
								"SOURCETABLE ", (const char*)&buf[0], 12))
						{ http_code = ::atoi((const char*)&buf[12]); }
						else
						{
							if (output)
								output.value().get().errormsg =
									"Server didn't send an HTTP/1.1 answer.";
							return net::http_errorcode::OtherHTTPError;
						}
					}

					// Check the HTTP code and the content-length:
					content_offset = pos_dblret + 4;

					// Do we have a "Content-Length:"??
					const char* ptr_len = ::strstr(
						reinterpret_cast<const char*>(&buf[0]),
						"Content-Length:");
					if (ptr_len)
					{
						content_length = ::atol(ptr_len + 15);
						content_length_read = true;
					}

					// Parse the rest of HTTP headers:
					{
						string aux_all_headers;
						deque<string> lstLines;
						aux_all_headers.resize(content_offset);
						std::memcpy(
							&aux_all_headers[0], &buf[0], content_offset);

						mrpt::system::tokenize(
							aux_all_headers, "\r\n", lstLines);

						for (auto i = lstLines.begin(); i != lstLines.end();
							 ++i)
						{
							const size_t p = i->find(":");
							if (p == string::npos) continue;

							const string key = i->substr(0, p);
							const string val = i->substr(p + 2);
							rx_headers[key] = val;
						}
					}
				}
			}
		}  // end while

		if (output) output.value().get().http_responsecode = http_code;
		if (output) output.value().get().out_headers = rx_headers;

		// Remove the headers from the content:
		buf.erase(buf.begin(), buf.begin() + content_offset);

		// Process: "Transfer-Encoding: chunked"
		if (0 != rx_headers.count("Transfer-Encoding") &&
			rx_headers.at("Transfer-Encoding") == "chunked")
		{
			// See: http://en.wikipedia.org/wiki/Chunked_transfer_encoding

			size_t index = 0;
			while (index < buf.size())
			{
				if (buf[index] == '\r' && buf[index + 1] == '\n')
				{
					buf.erase(buf.begin() + index, buf.begin() + index + 2);
					continue;
				}

				const char* pCRLF = ::strstr((const char*)&buf[index], "\r\n");
				if (!pCRLF) break;

				const size_t len_substr = ((char*)pCRLF) - (char*)(&buf[index]);

				string sLen((const char*)&buf[index], len_substr);
				sLen = string("0x") + sLen;

				unsigned int lenChunk;
				int fields = ::sscanf(sLen.c_str(), "%x", &lenChunk);
				if (!fields) break;

				// Remove the len of this chunk header from the data:
				buf.erase(
					buf.begin() + index, buf.begin() + index + len_substr + 2);

				index += lenChunk;

				if (!lenChunk)
				{
					buf.resize(index);
					break;
				}
			}
		}

		// Set the content output:
		out_content.swap(buf);

		if (http_code == 200) { return net::http_errorcode::Ok; }
		else
		{
			if (output)
				output.value().get().errormsg =
					format("HTTP error %i", http_code);
			return net::http_errorcode::OtherHTTPError;
		}
	}
	catch (const std::exception& e)
	{
		if (output) output.value().get().errormsg = e.what();
		return net::http_errorcode::CouldntConnect;
	}

	return net::http_errorcode::Ok;
}

/*---------------------------------------------------------------
	http_get
---------------------------------------------------------------*/
http_errorcode mrpt::comms::net::http_get(
	const string& url, std::vector<uint8_t>& out_content,
	const HttpRequestOptions& options,
	mrpt::optional_ref<HttpRequestOutput> output)
{
	return http_request("GET", "", url, out_content, options, output);
}

/** Resolve a server address by its name, returning its IP address as a string -
 * This method has a timeout for the maximum time to wait for the DNS server.
 *   For example: server_name="www.google.com" -> out_ip="209.85.227.99"
 *
 * \return true on success, false on timeout or other error.
 */
bool net::DNS_resolve_async(
	const std::string& server_name, std::string& out_ip,
	const unsigned int timeout_ms)
{
	// Firstly: If it's a numeric address already, do nothing:
	if (server_name.find_first_not_of("0123456789. ") == std::string::npos)
	{
		// It's a pure IP address:
		out_ip = server_name;
		return true;
	}

	// Solve DNS --------------
	// It seems that the only reliable way of *with a timeout* is to launch a
	// separate thread.

	std::future<std::string> dns_result_fut =
		std::async(std::launch::async, [&]() {
// Windows-specific stuff:
#ifdef _WIN32
			{
				// Init the WinSock Library:
				WORD wVersionRequested = MAKEWORD(2, 0);
				WSADATA wsaData;
				if (WSAStartup(wVersionRequested, &wsaData))
				{
					std::cerr
						<< "thread_DNS_solver_async: Error calling WSAStartup";
					return std::string();
				}
			}
#endif

			// Do the DNS lookup:
			std::string dns_result;

			hostent* he = gethostbyname(server_name.c_str());
			if (!he)
			{
				dns_result.clear();	 // empty string -> error.
			}
			else
			{
				struct in_addr ADDR;
				std::memcpy(
					&ADDR, he->h_addr,
					sizeof(ADDR));	// Was: *((struct in_addr *)he->h_addr);
				// Convert address to text:
				dns_result = string(inet_ntoa(ADDR));
			}

#ifdef _WIN32
			WSACleanup();
#endif
			return dns_result;
		});

	auto status =
		dns_result_fut.wait_for(std::chrono::milliseconds(timeout_ms));

	if (status == std::future_status::ready)
	{
		// Done: Anyway, it can still be an error result:
		out_ip = dns_result_fut.get();
		return !out_ip.empty();
	}
	else
	{
		// Timeout:
		out_ip.clear();

		return false;
	}
}

/** Returns a description of the last Sockets error */
std::string mrpt::comms::net::getLastSocketErrorStr()
{
#ifdef _WIN32
	const int errnum = WSAGetLastError();
	char* s = nullptr;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s,
		0, nullptr);
	const std::string str = mrpt::format("%s [errno=%d]", s, errnum);
	LocalFree(s);
	return str;
#else
	return std::string(strerror(errno));
#endif
}

bool mrpt::comms::net::Ping(
	const std::string& address, const int max_attempts,
	std::string* output_str /*=NULL*/)
{
	using namespace std;

	// Format a command string
	string cmd_str = "ping";

// different "count" argument for Windows and *NIX  systems
#if defined(MRPT_OS_LINUX) || defined(__APPLE__)
	cmd_str += " -c ";
#else
	cmd_str += " -n ";
#endif
	cmd_str += std::to_string(max_attempts);

	// Address:
	cmd_str += " ";
	cmd_str += address;

// Redirection:
#if defined(MRPT_OS_LINUX) || defined(__APPLE__)
	cmd_str += " 2>&1";
#endif

	// Finally exec the command
	int code = executeCommand(cmd_str, output_str);

	return (code == 0);
}
