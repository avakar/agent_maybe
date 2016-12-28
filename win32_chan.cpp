#include "chan.hpp"
#include <list>
#include <memory>
#include <algorithm>
#include "win32_error.hpp"
#include <windows.h>

namespace {

struct chan
	: istream, ostream
{
	chan();
	~chan();

	size_t read(char * buf, size_t len) override;
	size_t write(char const * buf, size_t len) override;

	void add_coroutine(std::function<void()> co);

	struct coroutine
	{
		std::function<void()> fn;
		void * fiber;

		union
		{
			char * rbuf;
			char const * wbuf;
		};

		size_t buf_size;
	};

	// The current is at the front.
	std::list<coroutine> ready;
	std::list<coroutine> readers;
	std::list<coroutine> writers;
	std::list<coroutine> cleanup;

	void clean();
	static void CALLBACK fiber_entry(void * param);

	chan(chan && o) = delete;
	chan & operator=(chan && o) = delete;
};

}

chan::chan()
{
	void * self = ConvertThreadToFiber(nullptr);
	if (self == nullptr)
	{
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_FIBER)
			self = GetCurrentFiber();
		else
			throw win32_error(GetLastError());
	}

	ready.push_back(coroutine());
	ready.back().fiber = self;
}

chan::~chan()
{
	while (ready.size() > 1)
	{
		ready.splice(ready.begin(), ready, ready.end());
		SwitchToFiber(ready.front().fiber);
		this->clean();
	}

	assert(readers.empty());
	assert(writers.empty());
}

size_t chan::read(char * buf, size_t len)
{
	if (!writers.empty())
	{
		auto & wr = writers.front();
		len = (std::min)(len, wr.buf_size);
		memcpy(buf, wr.wbuf, len);
		wr.buf_size = len;

		ready.splice(ready.end(), writers, writers.begin());
		return len;
	}

	// Move ourselves to the reader list and activate the first ready coroutine.
	if (ready.size() < 2)
		return 0;

	ready.front().rbuf = buf;
	ready.front().buf_size = len;
	readers.splice(readers.end(), ready, ready.begin());

	SwitchToFiber(ready.front().fiber);
	clean();

	return ready.front().buf_size;
}

size_t chan::write(char const * buf, size_t len)
{
	if (!readers.empty())
	{
		auto & rd = readers.front();
		len = (std::min)(len, rd.buf_size);
		memcpy(rd.rbuf, buf, len);
		rd.buf_size = len;

		ready.splice(ready.end(), readers, readers.begin());
		return len;
	}

	// Move ourselves to the writer list and activate the first ready coroutine.
	if (ready.size() < 2)
		throw std::runtime_error("can't write, no readers available");

	ready.front().wbuf = buf;
	ready.front().buf_size = len;
	writers.splice(writers.end(), ready, ready.begin());

	SwitchToFiber(ready.front().fiber);
	this->clean();

	size_t r = ready.front().buf_size;
	if (r == 0)
		throw std::runtime_error("broken channel");

	return r;
}

void CALLBACK chan::fiber_entry(void * param)
{
	chan * self = static_cast<chan *>(param);

	try
	{
		(self->ready.front().fn)();
	}
	catch (...)
	{
		// TODO
	}

	self->cleanup.splice(self->cleanup.end(), self->ready, self->ready.begin());

	void * target;

	if (!self->ready.empty())
	{
		target = self->ready.front().fiber;
	}
	else if (!self->readers.empty())
	{
		self->ready.splice(self->ready.begin(), self->readers, self->readers.begin());

		auto & rd = self->ready.front();
		rd.buf_size = 0;
		target = rd.fiber;
	}
	else
	{
		assert(!self->writers.empty());
		self->ready.splice(self->ready.begin(), self->writers, self->writers.begin());

		auto & rd = self->ready.front();
		rd.buf_size = 0;
		target = rd.fiber;
	}

	SwitchToFiber(target);
}

void chan::add_coroutine(std::function<void()> co)
{
	ready.push_back(coroutine());

	void * fiber = CreateFiber(32*1024, &chan::fiber_entry, this);

	if (fiber == nullptr)
	{
		ready.pop_back();
		throw win32_error(GetLastError());
	}

	ready.back().fiber = fiber;
	ready.back().fn = std::move(co);
}

void chan::clean()
{
	for (auto && co : cleanup)
		DeleteFiber(co.fiber);
	cleanup.clear();
}

std::shared_ptr<istream> make_istream(std::function<void(ostream & out)> fn)
{
	struct ctx
	{
		std::function<void(ostream & out)> fn;
		chan ch;

		ctx(std::function<void(ostream & out)> && fn)
			: fn(std::move(fn))
		{
		}
	};

	auto px = std::make_shared<ctx>(std::move(fn));
	auto pp = px.get();

	px->ch.add_coroutine([pp]() {
		pp->fn(pp->ch);
	});

	return std::shared_ptr<istream>(px, &px->ch);
}
