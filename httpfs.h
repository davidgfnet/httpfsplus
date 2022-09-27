
#include <nlohmann/json.hpp>
#include <map>

#include "lrucache.h"
#include "httpclient.h"

class HttpFSServer {
public:
	HttpFSServer(std::string url, unsigned metacachettl);

	class DirEntry {
	public:
		std::map<std::string, struct stat> entries;
		time_t fetch_time;
	};

	HttpClient metaclient;     // For getattr/readdir-like operations
	HttpClient readclient;     // For data transfer operations

	bool readDir(std::string path, DirEntry &entry);
	int readBlock(std::string path, char *buf, uint64_t offset, uint64_t size);

private:
	typedef lru11::Cache<std::string, DirEntry, std::mutex> CacheType;

	const std::string url;
	const unsigned metacachettl;
	CacheType metacache;
};


