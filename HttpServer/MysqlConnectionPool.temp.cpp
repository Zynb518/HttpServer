#include <mysqlx/xdevapi.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
constexpr const char* kUriEnv = "MYSQLX_URI";
constexpr const char* kMinEnv = "MYSQLX_POOL_MIN";
constexpr const char* kMaxEnv = "MYSQLX_POOL_MAX";
constexpr const char* kQueueTimeoutEnv = "MYSQLX_POOL_QUEUE_TIMEOUT_MS";
constexpr const char* kIdleTimeoutEnv = "MYSQLX_POOL_IDLE_TIMEOUT_MS";

constexpr const char* kDefaultUri = "mysqlx://httpserver:114514ZY@localhost/scut_sims";
constexpr std::size_t kDefaultMinSize = 2;
constexpr std::size_t kDefaultMaxSize = 10;
constexpr std::chrono::milliseconds kDefaultQueueTimeout{2000};
constexpr std::chrono::milliseconds kDefaultIdleTimeout{60000};

std::size_t parseSizeOption(const char* value, std::size_t fallback, const char* name)
{
	if (!value || !*value)
	{
		return fallback;
	}

	std::size_t result = fallback;
	try
	{
		result = static_cast<std::size_t>(std::stoul(value));
	}
	catch (const std::exception&)
	{
		std::cerr << "[MysqlConnectionPool] 环境变量 " << name << " 解析失败，使用默认值 " << fallback << "\n";
	}
	return result;
}

std::chrono::milliseconds parseDurationOption(const char* value, std::chrono::milliseconds fallback, const char* name)
{
	if (!value || !*value)
	{
		return fallback;
	}

	std::chrono::milliseconds result = fallback;
	try
	{
		result = std::chrono::milliseconds(std::stoul(value));
	}
	catch (const std::exception&)
	{
		std::cerr << "[MysqlConnectionPool] 环境变量 " << name << " 解析失败，使用默认值 "
				  << fallback.count() << "ms\n";
	}
	return result;
}

std::string resolveUri()
{
	const char* env = std::getenv(kUriEnv);
	if (env && *env)
	{
		return env;
	}
	std::cerr << "[MysqlConnectionPool] 未检测到环境变量 " << kUriEnv << "，使用默认 URI\n";
	return kDefaultUri;
}
} // namespace

class SessionHandle
{
public:
	SessionHandle() = default;
	explicit SessionHandle(mysqlx::Session&& session) noexcept
		: _session(std::move(session))
	{
	}

	SessionHandle(const SessionHandle&) = delete;
	SessionHandle& operator=(const SessionHandle&) = delete;

	SessionHandle(SessionHandle&& other) noexcept
		: _session(std::move(other._session))
	{
	}

	SessionHandle& operator=(SessionHandle&& other) noexcept
	{
		if (this != &other)
		{
			close();
			_session = std::move(other._session);
		}
		return *this;
	}

	~SessionHandle()
	{
		close();
	}

	mysqlx::Session& operator*()
	{
		return _session;
	}

	mysqlx::Session* operator->()
	{
		return &_session;
	}

	const mysqlx::Session& operator*() const
	{
		return _session;
	}

	const mysqlx::Session* operator->() const
	{
		return &_session;
	}

	bool valid() const
	{
		return _session.isOpen();
	}

	void close()
	{
		if (_session.isOpen())
		{
			try
			{
				_session.close();
			}
			catch (const mysqlx::Error& err)
			{
				std::cerr << "[MysqlConnectionPool] 归还连接时关闭失败: " << err << "\n";
			}
			catch (const std::exception& err)
			{
				std::cerr << "[MysqlConnectionPool] 归还连接时遇到异常: " << err.what() << "\n";
			}
		}
	}

private:
	mysqlx::Session _session;
};

class MysqlConnectionPool
{
public:
	static MysqlConnectionPool& Instance()
	{
		static MysqlConnectionPool instance;
		return instance;
	}

	SessionHandle Acquire()
	{
		return SessionHandle(getSessionInternal());
	}

	mysqlx::Session getSession()
	{
		return getSessionInternal();
	}

	std::string uri() const
	{
		return _uri;
	}

private:
	MysqlConnectionPool()
		: _uri(resolveUri()),
		  _minSize(parseSizeOption(std::getenv(kMinEnv), kDefaultMinSize, kMinEnv)),
		  _maxSize(parseSizeOption(std::getenv(kMaxEnv), kDefaultMaxSize, kMaxEnv)),
		  _queueTimeout(parseDurationOption(std::getenv(kQueueTimeoutEnv), kDefaultQueueTimeout, kQueueTimeoutEnv)),
		  _idleTimeout(parseDurationOption(std::getenv(kIdleTimeoutEnv), kDefaultIdleTimeout, kIdleTimeoutEnv)),
		  _client(_uri,
				  mysqlx::ClientOption::POOL_MIN_SIZE, static_cast<unsigned int>(_minSize),
				  mysqlx::ClientOption::POOL_MAX_SIZE, static_cast<unsigned int>(_maxSize),
				  mysqlx::ClientOption::POOL_QUEUE_TIMEOUT, _queueTimeout,
				  mysqlx::ClientOption::POOL_IDLE_TIMEOUT, _idleTimeout)
	{
		if (_minSize > _maxSize)
		{
			throw std::invalid_argument("MysqlConnectionPool: POOL_MIN_SIZE 大于 POOL_MAX_SIZE");
		}

		try
		{
			warmup();
		}
		catch (const mysqlx::Error& err)
		{
			std::ostringstream oss;
			oss << "[MysqlConnectionPool] 连接池预热失败: " << err;
			throw std::runtime_error(oss.str());
		}
		catch (const std::exception& err)
		{
			std::ostringstream oss;
			oss << "[MysqlConnectionPool] 连接池预热异常: " << err.what();
			throw std::runtime_error(oss.str());
		}
	}

	mysqlx::Session getSessionInternal()
	{
		try
		{
			return _client.getSession();
		}
		catch (const mysqlx::Error& err)
		{
			std::ostringstream oss;
			oss << "[MysqlConnectionPool] 获取连接失败: " << err;
			throw std::runtime_error(oss.str());
		}
	}

	void warmup()
	{
		const auto target = std::min<std::size_t>(_minSize, _maxSize);
		for (std::size_t i = 0; i < target; ++i)
		{
			mysqlx::Session session = _client.getSession();
			session.close();
		}
	}

private:
	std::string _uri;
	std::size_t _minSize;
	std::size_t _maxSize;
	std::chrono::milliseconds _queueTimeout;
	std::chrono::milliseconds _idleTimeout;
	mysqlx::Client _client;
};
