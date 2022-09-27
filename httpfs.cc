
#include <nlohmann/json.hpp>
#include <unistd.h>

#include "httpfs.h"

static const char *hcharset = "0123456789abcdef";
static std::string urienc(std::string s) {
	std::string ret;
	for (char c : s) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			ret += c;
		else {
			ret.push_back('%');
			ret.push_back(hcharset[(*(uint8_t*)&c) >> 4]);
			ret.push_back(hcharset[(*(uint8_t*)&c) & 15]);
		}
	}
	return ret;
}

HttpFSServer::HttpFSServer(std::string url, unsigned metacachettl)
 : url(url), metacachettl(metacachettl), metacache(4*1024, 512)
{
}

static HttpFSServer::DirEntry parse_response(nlohmann::json jresp) {
	HttpFSServer::DirEntry entry;
	entry.fetch_time = time(NULL);
	for (unsigned i = 0; i < jresp.size(); i++) {
		auto fname = jresp[i]["name"].get<std::string>();
		bool isdir = jresp[i]["type"] == "directory";
		struct tm pdate;
		strptime(jresp[i]["mtime"].get<std::string>().c_str(), "%a, %d %b %Y %H:%M:%S %Z", &pdate);
		time_t mtime = mktime(&pdate);

		struct stat fst;
		memset(&fst, 0, sizeof(fst));
		fst.st_ino = 0;   // TODO: Needed if -o use_ino is used!?
		fst.st_mode = S_IRUSR | S_IRGRP | (isdir ? S_IFDIR : S_IFREG);
		fst.st_atime = mtime;
		fst.st_mtime = mtime;
		fst.st_ctime = mtime;
		fst.st_nlink = 1;
		fst.st_uid = getuid();
		fst.st_gid = getgid();
		if (!isdir)
			fst.st_size = jresp[i]["size"].get<uint64_t>();
		entry.entries[fname] = fst;
	}
	return entry;
}

bool HttpFSServer::readDir(std::string path, DirEntry &entry) {
	// Check the cache
	if (metacache.tryGet(path, entry)) {
		if (entry.fetch_time > time(NULL) - metacachettl) {
			// Pre-fetch (async) any entry that is close to expire
			if (entry.fetch_time < time(NULL) - metacachettl/2) {
				auto jsresp = std::make_shared<std::string>();
				readclient.doGET(url + urienc(path), 0, 0,
					[jsresp] (std::string data) -> bool {
						*jsresp += data;
						return true;
					},
					[jsresp, path, this] (bool ok) {
						auto js = nlohmann::json::parse(*jsresp, nullptr, false);
						if (ok && !js.is_discarded())
							metacache.insert(path, parse_response(js));
					});
			}
			return true;    // Still cached, still valid
		}
		else
			metacache.remove(path);    // Entry has expired, re-fetch
	}

	auto ret = readclient.get(url + urienc(path), 0, 0);
	if (!ret.first)
		return false;

	auto jresp = nlohmann::json::parse(ret.second, nullptr, false);
	if (jresp.is_discarded())
		return false;

	entry = parse_response(jresp);

	// Cache fill
	metacache.insert(path, entry);
	return true;
}

int HttpFSServer::readBlock(std::string path, char *buf, uint64_t offset, uint64_t size) {
	auto ret = readclient.get(url + urienc(path), offset, size);
	if (!ret.first || ret.second.size() > size)
		return -1;

	memcpy(buf, &ret.second[0], ret.second.size());
	return ret.second.size();
}

