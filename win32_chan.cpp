#include "chan.hpp"
#include <memory>
#include <algorithm>
#include "win32_error.hpp"
#include <windows.h>

namespace {

struct chan final
	: istream, private ostream
{
	explicit chan(std::function<void(ostream & out)> coroutine);
	~chan();
	chan(chan && o) = delete;
	chan & operator=(chan && o) = delete;

	size_t read(char * buf, size_t len) override;

private:
	std::function<void(ostream & out)> coroutine_;
	void * fiber_;
	char * rd_buf_;
	size_t rd_len_;

	void switch_to_reader();
	void switch_to_writer();
	size_t write(char const * buf, size_t len) override;

	static void CALLBACK fiber_entry(void * param);
};

}

chan::chan(std::function<void(ostream & out)> coroutine)
	: coroutine_(std::move(coroutine)), fiber_(nullptr)
{
}

chan::~chan()
{
	if (fiber_ != nullptr)
	{
		rd_buf_ = nullptr;
		rd_len_ = 0;
		this->switch_to_writer();

		DeleteFiber(fiber_);
	}
}

void chan::switch_to_reader()
{
	assert(fiber_ != nullptr);

	void * self = GetCurrentFiber();
	std::swap(fiber_, self);
	SwitchToFiber(self);
}

void chan::switch_to_writer()
{
	if (fiber_ == nullptr)
	{
		fiber_ = CreateFiber(32 * 1024, &chan::fiber_entry, this);
		if (fiber_ == nullptr)
			throw win32_error(GetLastError());
	}

	void * self = ConvertThreadToFiber(nullptr);
	if (self == nullptr)
	{
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_FIBER)
			self = GetCurrentFiber();
		else
			throw win32_error(GetLastError());
	}

	std::swap(fiber_, self);
	SwitchToFiber(self);
}

size_t chan::read(char * buf, size_t len)
{
	rd_buf_ = buf;
	rd_len_ = len;
	this->switch_to_writer();

	return rd_len_;
}

size_t chan::write(char const * buf, size_t len)
{
	if (len == 0)
		return 0;

	if (rd_len_ == 0)
		throw std::runtime_error("epipe");

	if (rd_len_ < len)
		len = rd_len_;

	memcpy(rd_buf_, buf, len);
	rd_len_ = len;

	this->switch_to_reader();
	return len;
}

void CALLBACK chan::fiber_entry(void * param)

{
	chan * self = static_cast<chan *>(param);

	try
	{
		(self->coroutine_)(*self);
	}
	catch (...)
	{
		// TODO
	}

	for (;;)
	{
		self->rd_len_ = 0;
		self->switch_to_reader();
	}
}

std::shared_ptr<istream> make_istream(std::function<void(ostream & out)> fn)
{
	auto px = std::make_shared<chan>(std::move(fn));
	return std::shared_ptr<istream>(px, px.get());
}
