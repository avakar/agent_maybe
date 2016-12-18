#include "chan.hpp"
#include <list>
#include <memory>
#include <algorithm>
#include "win32_error.hpp"
#include <windows.h>

struct chan::impl
{
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
};

chan::chan()
	: pimpl_(new impl())
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

	pimpl_->ready.push_back(impl::coroutine());
	pimpl_->ready.back().fiber = self;
}

chan::chan(chan && o)
	: pimpl_(o.pimpl_)
{
	o.pimpl_ = nullptr;
}

chan::~chan()
{
	if (!pimpl_)
		return;

	while (pimpl_->ready.size() > 1)
	{
		pimpl_->ready.splice(pimpl_->ready.begin(), pimpl_->ready, pimpl_->ready.end());
		SwitchToFiber(pimpl_->ready.front().fiber);
		pimpl_->clean();
	}

	assert(pimpl_->readers.empty());
	assert(pimpl_->writers.empty());

	delete pimpl_;
}

chan & chan::operator=(chan && o)
{
	using std::swap;
	swap(pimpl_, o.pimpl_);
	return *this;
}

size_t chan::read(char * buf, size_t len)
{
	if (!pimpl_->writers.empty())
	{
		auto & wr = pimpl_->writers.front();
		len = (std::min)(len, wr.buf_size);
		memcpy(buf, wr.wbuf, len);
		wr.buf_size = len;

		pimpl_->ready.splice(pimpl_->ready.end(), pimpl_->writers, pimpl_->writers.begin());
		return len;
	}

	// Move ourselves to the reader list and activate the first ready coroutine.
	if (pimpl_->ready.size() < 2)
		return 0;

	pimpl_->ready.front().rbuf = buf;
	pimpl_->ready.front().buf_size = len;
	pimpl_->readers.splice(pimpl_->readers.end(), pimpl_->ready, pimpl_->ready.begin());

	SwitchToFiber(pimpl_->ready.front().fiber);
	pimpl_->clean();

	return pimpl_->ready.front().buf_size;
}

size_t chan::write(char const * buf, size_t len)
{
	if (!pimpl_->readers.empty())
	{
		auto & rd = pimpl_->readers.front();
		len = (std::min)(len, rd.buf_size);
		memcpy(rd.rbuf, buf, len);
		rd.buf_size = len;

		pimpl_->ready.splice(pimpl_->ready.end(), pimpl_->readers, pimpl_->readers.begin());
		return len;
	}

	// Move ourselves to the writer list and activate the first ready coroutine.
	if (pimpl_->ready.size() < 2)
		throw std::runtime_error("can't write, no readers available");

	pimpl_->ready.front().wbuf = buf;
	pimpl_->ready.front().buf_size = len;
	pimpl_->writers.splice(pimpl_->writers.end(), pimpl_->ready, pimpl_->ready.begin());

	SwitchToFiber(pimpl_->ready.front().fiber);
	pimpl_->clean();

	size_t r = pimpl_->ready.front().buf_size;
	if (r == 0)
		throw std::runtime_error("broken channel");

	return r;
}

void CALLBACK chan::impl::fiber_entry(void * param)
{
	impl * pimpl = static_cast<impl *>(param);

	try
	{
		(pimpl->ready.front().fn)();
	}
	catch (...)
	{
		// TODO
	}

	pimpl->cleanup.splice(pimpl->cleanup.end(), pimpl->ready, pimpl->ready.begin());

	void * target;

	if (!pimpl->ready.empty())
	{
		target = pimpl->ready.front().fiber;
	}
	else if (!pimpl->readers.empty())
	{
		pimpl->ready.splice(pimpl->ready.begin(), pimpl->readers, pimpl->readers.begin());

		auto & rd = pimpl->ready.front();
		rd.buf_size = 0;
		target = rd.fiber;
	}
	else
	{
		assert(!pimpl->writers.empty());
		pimpl->ready.splice(pimpl->ready.begin(), pimpl->writers, pimpl->writers.begin());

		auto & rd = pimpl->ready.front();
		rd.buf_size = 0;
		target = rd.fiber;
	}

	SwitchToFiber(target);
}

void chan::add_coroutine(std::function<void()> co)
{
	pimpl_->ready.push_back(impl::coroutine());

	void * fiber = CreateFiber(32*1024, &chan::impl::fiber_entry, pimpl_);

	if (fiber == nullptr)
	{
		pimpl_->ready.pop_back();
		throw win32_error(GetLastError());
	}

	pimpl_->ready.back().fiber = fiber;
	pimpl_->ready.back().fn = std::move(co);
}

void chan::impl::clean()
{
	for (auto && co : cleanup)
		DeleteFiber(co.fiber);
	cleanup.clear();
}
