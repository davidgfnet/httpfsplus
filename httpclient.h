
// Implements a HTTPs client that works in async fashion.
// It is able to perform requests and add requests on the fly.
// Uses libcurl as backend.

#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <memory>
#include <map>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <thread>
#include <functional>
#include <future>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <curl/curl.h>

#define CONNECT_TIMEOUT    30   // We will retry, but that sounds like a lot
#define TRANSFER_TIMEOUT   60   // Abort after a minute, not even uploads are that slow

typedef size_t(*curl_write_function)(char *ptr, size_t size, size_t nmemb, void *userdata);

class HttpClient {
private:
	class t_query {
	public:
		t_query() : headers(NULL) {}
		~t_query() {
			if (headers)
				curl_slist_free_all(headers);
		}
		std::function<bool(std::string)> wrcb;  // Write callback (data download)
		std::function<void(bool)>      donecb;  // End callback with result
		struct curl_slist *headers;             // Any needed headers
	};
	// Client thread
	std::thread worker;
	// Queue of pending requests to be performed
	std::map<CURL*, std::unique_ptr<t_query>> rqueue;
	mutable std::mutex rqueue_mutex;
	// Multi handlers that is in charge of doing requests.
	CURLM *multi_handle;
	std::map<CURL*, std::unique_ptr<t_query>> request_set;
	// Signal end of thread
	bool end;
	// Proxy
	std::string proxy_addr;
	// Timeouts
	unsigned connto, tranfto;
	// Pipe used to signal select() end, so we can add new requests
	int pipefd[2];

public:

	HttpClient(
		std::string proxy_addr = "",
		unsigned connto = CONNECT_TIMEOUT,
		unsigned tranfto = TRANSFER_TIMEOUT
	)
	: multi_handle(curl_multi_init()), end(false),
	  proxy_addr(proxy_addr), connto(connto), tranfto(tranfto) {

		// Create a new pipe, make both ends non-blocking
		if (pipe(pipefd) < 0)
			throw std::system_error();

		for (unsigned i = 0; i < 2; i++) {
			int flags = fcntl(pipefd[i], F_GETFL);
			if (flags < 0)
				throw std::system_error();
			if (fcntl(pipefd[i], F_SETFL, flags | O_NONBLOCK) < 0)
				throw std::system_error();
		}

		// Start thread
		worker = std::thread(&HttpClient::work, this);
	}

	~HttpClient() {
		// Mark it as done
		end = true;

		// Unblock the thread
		(void)write(pipefd[1], "\0", 1);

		// Now detroy the thread
		worker.join();

		// Close the signaling pipe
		close(pipefd[0]);
		close(pipefd[1]);

		// Manually cleanup any easy handles inflight or pending
		for (const auto & req: request_set) {
			curl_multi_remove_handle(multi_handle, req.first);
			curl_easy_cleanup(req.first);
		}
		for (const auto & req: rqueue) {
			curl_multi_remove_handle(multi_handle, req.first);
			curl_easy_cleanup(req.first);
		}

		// Wipe multi
		curl_multi_cleanup(multi_handle);
	}

	std::pair<bool, std::string> get(const std::string &url, uint64_t offset, uint64_t maxsize) {

		// Use the async interface and block until ready
		std::string response;
		std::promise<bool> p;
		this->doGET(url, offset, maxsize,
			[&response] (std::string chunk) -> bool {
				response += chunk;
				return true;
			},
			[&p] (bool success) {
				p.set_value(success);
			});
		return {p.get_future().get(), response};
	}

	void doGET(const std::string &url,
		uint64_t offset, uint64_t maxsize,
		std::function<bool(std::string)> wrcb = nullptr,
		std::function<void(bool)> donecb = nullptr) {

		CURL *req = curl_easy_init();
		auto userq = std::make_unique<t_query>();
		userq->wrcb = std::move(wrcb);
		userq->donecb = std::move(donecb);
		curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(req, CURLOPT_AUTOREFERER, 1L);
		curl_easy_setopt(req, CURLOPT_MAXREDIRS, 5);

		if (maxsize || offset) {
			std::string rangedsc = std::to_string(offset) + "-" + std::to_string(offset+maxsize-1);
			curl_easy_setopt(req, CURLOPT_RANGE, rangedsc.c_str());
		}

		this->doQuery(req, url, std::move(userq));
	}

	void doQuery(CURL *req, std::string url, std::unique_ptr<t_query> userq) {
		curl_write_function wrapperfn{[]
			(char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
				// Push data to the user-defined callback if any
				t_query *q = static_cast<t_query*>(userdata);
				if (q->wrcb && !q->wrcb(std::string(ptr, size*nmemb)))
					return 0;
				return size * nmemb;
			}
		};

		curl_easy_setopt(req, CURLOPT_URL, url.c_str());
		curl_easy_setopt(req, CURLOPT_CONNECTTIMEOUT, connto);
		curl_easy_setopt(req, CURLOPT_TIMEOUT, tranfto);
		curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, wrapperfn);
		curl_easy_setopt(req, CURLOPT_WRITEDATA, userq.get());
		if (!proxy_addr.empty())
			curl_easy_setopt(req, CURLOPT_PROXY, proxy_addr.c_str());

		// Disable 100 continue requests
		userq->headers = curl_slist_append(userq->headers, "Expect:");
		curl_easy_setopt(req, CURLOPT_HTTPHEADER, userq->headers);

		// Enqueues a query in the pending queue
		{
			std::lock_guard<std::mutex> guard(rqueue_mutex);
			rqueue[req] = std::move(userq);
		}

		// Use self-pipe trick to make select return immediately
		(void)write(pipefd[1], "\0", 1);
	}

	// Will process http client requests
	void work() {
		while (!end) {
			// Process input queue to add new requests
			{
				std::lock_guard<std::mutex> guard(rqueue_mutex);
				for (auto & req: rqueue) {
					// Add to the Multi client
					curl_multi_add_handle(multi_handle, req.first);
					// Add it to the req_set
					request_set[req.first] = std::move(req.second);
				}
				rqueue.clear();
			}

			// Work a bit, non blocking fashion
			int stillrun;
			curl_multi_perform(multi_handle, &stillrun);

			// Retrieve events to care about (cleanup of finished reqs)
			int msgs_left = 0, msg_proc = 0;
			CURLMsg *msg;
			while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
				if (msg->msg == CURLMSG_DONE) {
					CURL *h = msg->easy_handle;
					bool okcode = (msg->data.result == CURLE_OK);
					curl_multi_remove_handle(multi_handle, h);
					curl_easy_cleanup(h);

					// Call completion callback
					if (request_set.count(h)) {
						const auto &uq = request_set.at(h);
						if (uq->donecb)
							uq->donecb(okcode);
						request_set.erase(h);
					}
				}
				msg_proc++;
			}

			if (!msg_proc && !end) {
				struct timeval timeout;
				timeout.tv_sec = 10;
				timeout.tv_usec = 0;

				int maxfd;
				fd_set rd, wr, er;
				FD_ZERO(&rd); FD_ZERO(&wr); FD_ZERO(&er);
				FD_SET(pipefd[0], &rd);

				curl_multi_fdset(multi_handle, &rd, &wr, &er, &maxfd);
				maxfd = std::max(maxfd, pipefd[0]);

				// Wait for something to happen, or the pipe to unblock us
				// or just timeout after some time, just in case.
				select(maxfd+1, &rd, &wr, &er, &timeout);

				char tmp[1024];
				(void)read(pipefd[0], tmp, sizeof(tmp));
			}
		}
	}
};

#endif

