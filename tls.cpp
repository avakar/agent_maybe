#include "tls.hpp"
#include <internal/bio.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <exception>

namespace {

struct stream_bio final
{
	explicit stream_bio(void * stream)
		: stream_(stream)
	{
		static BIO_METHOD const meth = {
			BIO_TYPE_SOURCE_SINK,
			"cpp_stream_bio",

			&bwrite,
			&bread,
			nullptr,
			nullptr,
			&ctrl,
		};

		bio_ = BIO_new(&meth);
		if (bio_ == nullptr)
			throw std::bad_alloc();

		BIO_set_data(bio_, this);
		BIO_set_init(bio_, 1);
	}

	~stream_bio()
	{
		if (bio_)
			BIO_vfree(bio_);
	}

	void detach()
	{
		bio_ = nullptr;
	}

	void rethrow()
	{
		if (err_)
			std::rethrow_exception(err_);
	}

	BIO * get() const
	{
		return bio_;
	}

private:
	BIO * bio_;
	void * stream_;
	std::exception_ptr err_;

	static int bwrite(BIO * bio, const char * buf, int len)
	{
		auto self = static_cast<stream_bio *>(BIO_get_data(bio));
		if (self->err_)
			return -1;

		auto ss = static_cast<ostream *>(self->stream_);

		try
		{
			return (int)ss->write(buf, len);
		}
		catch (...)
		{
			self->err_ = std::current_exception();
			return -1;
		}
	}

	static int bread(BIO * bio, char * buf, int len)
	{
		auto self = static_cast<stream_bio *>(BIO_get_data(bio));
		if (self->err_)
			return -1;

		auto ss = static_cast<istream *>(self->stream_);

		try
		{
			return (int)ss->read(buf, len);
		}
		catch (...)
		{
			self->err_ = std::current_exception();
			return -1;
		}
	}

	static long ctrl(BIO * bio, int cmd, long, void *)
	{
		if (cmd == BIO_CTRL_DUP || cmd == BIO_CTRL_FLUSH)
			return 1;
		return 0;
	}
};

struct ctx final
	: istream, ostream
{
	stream_bio rbio;
	stream_bio wbio;

	SSL_CTX * sslctx;
	SSL * ssl;

	ctx(istream & in, ostream & out, std::string const & key, std::string const & cert)
		: rbio(&in), wbio(&out), sslctx(nullptr), ssl(nullptr)
	{
		sslctx = SSL_CTX_new(TLSv1_2_server_method());
		if (!sslctx)
			throw std::bad_alloc();

		ssl = SSL_new(sslctx);
		if (!ssl)
		{
			SSL_CTX_free(sslctx);
			throw std::bad_alloc();
		}

		SSL_use_certificate_file(ssl, cert.c_str(), SSL_FILETYPE_PEM);
		SSL_use_PrivateKey_file(ssl, key.c_str(), SSL_FILETYPE_PEM);
		SSL_set_bio(ssl, rbio.get(), wbio.get());
		rbio.detach();
		wbio.detach();

		if (!SSL_accept(ssl))
		{
			SSL_free(ssl);
			SSL_CTX_free(sslctx);
			throw std::runtime_error("accept");
		}
	}

	~ctx()
	{
		if (ssl)
			SSL_free(ssl);
		if (sslctx)
			SSL_CTX_free(sslctx);
	}

	size_t read(char * buf, size_t len) override
	{
		int r = SSL_read(ssl, buf, len);
		if (r < 0)
		{
			rbio.rethrow();
			wbio.rethrow();
			throw std::runtime_error("read error");
		}
		return (size_t)r;
	}

	size_t write(char const * buf, size_t len) override
	{
		int r = SSL_write(ssl, buf, len);
		if (r <= 0)
		{
			rbio.rethrow();
			wbio.rethrow();
			throw std::runtime_error("write error");
		}
		return (size_t)r;
	}
};

}

void tls_server(std::shared_ptr<istream> & in_tls, std::shared_ptr<ostream> & out_tls, istream & in, ostream & out, std::string const & key, std::string const & cert)
{
	auto r = std::make_shared<ctx>(in, out, key, cert);
	in_tls = r;
	out_tls = r;
}
