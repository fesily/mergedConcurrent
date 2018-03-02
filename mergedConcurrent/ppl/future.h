#pragma once
#include <ppltasks.h>
#include <mutex>
#if _MSC_VER < 1900
namespace std{
#ifndef _NOEXCEPT
#define _NOEXCEPT throw()
#endif
// Stores a tuple of indices.  Used by tuple and pair, and by bind() to
// extract the elements in a tuple.
template<class _Ty,
	_Ty... _Vals>
	struct integer_sequence
{	// sequence of integer parameters
	static_assert(std::is_integral<_Ty>::value,
		"integer_sequence<T, I...> requires T to be an integral type.");

	typedef integer_sequence<_Ty, _Vals...> type;
	typedef _Ty value_type;

	static size_t size() _NOEXCEPT
	{	// get length of parameter list
		return (sizeof...(_Vals));
	}
};

// ALIAS TEMPLATE make_integer_sequence
template<bool _Negative,
	bool _Zero,
	class _Int_con,
	class _Int_seq>
	struct _Make_seq
{	// explodes gracefully below 0
	static_assert(!_Negative,
		"make_integer_sequence<T, N> requires N to be non-negative.");
};

template<class _Ty,
	_Ty... _Vals>
	struct _Make_seq<false, true,
	std::integral_constant<_Ty, 0>,
	integer_sequence<_Ty, _Vals...> >
	: integer_sequence<_Ty, _Vals...>
{	// ends recursion at 0
};

template<class _Ty,
	_Ty _Ix,
	_Ty... _Vals>
	struct _Make_seq<false, false,
	std::integral_constant<_Ty, _Ix>,
	integer_sequence<_Ty, _Vals...> >
	: _Make_seq<false, _Ix == 1,
	std::integral_constant<_Ty, _Ix - 1>,
	integer_sequence<_Ty, _Ix - 1, _Vals...> >
{	// counts down to 0
};

template<class _Ty, _Ty _Size>
using make_integer_sequence = typename _Make_seq < _Size < 0, _Size == 0, integral_constant<_Ty, _Size>, integer_sequence<_Ty> >::type;

template<size_t... _Vals>
using index_sequence = integer_sequence<size_t, _Vals...>;

template<size_t _Size>
using make_index_sequence = make_integer_sequence<size_t, _Size>;
}
#endif
namespace mergedConcurrent
{
template<typename T>
class future;
template<typename T>
class shared_future;
template<class T>
class packaged_task;

enum class future_errc {	// names for futures errors
	broken_promise = 1,
	future_already_retrieved,
	promise_already_satisfied,
	no_state
};
enum class future_status {	// names for timed wait function returns
	ready,
	timeout,
	deferred
};
inline const char *_Future_error_map(int _Errcode) _NOEXCEPT
{	// convert to name of future error
	switch (static_cast<future_errc>(_Errcode))
	{	// switch on error code value
	case future_errc::broken_promise:
		return ("broken promise");

	case future_errc::future_already_retrieved:
		return ("future already retrieved");

	case future_errc::promise_already_satisfied:
		return ("promise already satisfied");

	case future_errc::no_state:
		return ("no state");

	default:
		return (0);
	}
}
class _Future_error_category
	: public std::_Generic_error_category
{	// categorize a future error
public:
	_Future_error_category()
	{	// default constructor
	}

	virtual const char *name() const _NOEXCEPT
	{	// get name of category
		return ("future");
	}

	virtual std::string message(int _Errcode) const
	{	// convert to name of error
		const char *_Name = _Future_error_map(_Errcode);
		if (_Name != 0)
			return (_Name);
		else
			return (_Generic_error_category::message(_Errcode));
	}
};
inline const std::error_category& future_category() _NOEXCEPT
{	// return error_category object for future
	static _Future_error_category c;
	return c;
}
inline std::error_code make_error_code(future_errc _Errno) _NOEXCEPT
{	// make an error_code object
	return (std::error_code(static_cast<int>(_Errno), future_category()));
}
class future_error
	: public std::logic_error
{	// future exception
public:
	explicit future_error(std::error_code _Errcode) // internal, will be removed
		: logic_error(""), _Mycode(_Errcode)
	{	// construct from error code
	}

	explicit future_error(future_errc _Errno)
		: logic_error(""), _Mycode(make_error_code(_Errno))
	{	// construct from future_errc
	}

	const std::error_code& code() const _NOEXCEPT
	{	// return stored error code
		return (_Mycode);
	}

	virtual const char * __CLR_OR_THIS_CALL what() const _NOEXCEPT override
	{	// get message string
		return (_Future_error_map(_Mycode.value()));
	}
private:
	std::error_code _Mycode;	// the stored error code
};
#if _MSC_VER < 1900
struct future_help
{
	::Concurrency::extensibility::event_t _M_Scheduled;
	::Concurrency::extensibility::event_t _M_Completed;
	::Concurrency::details::_AsyncTaskCollection* _M_pTaskCollection;
	::Concurrency::scheduler_ptr _M_pScheduler;
};
static_assert(sizeof(::Concurrency::details::_TaskCollection_t) == sizeof(future_help), "");
#else
struct future_help
{
	enum _TaskCollectionState {
		_New,
		_Scheduled,
		_Completed
	};
	::std::condition_variable _M_StateChanged;
	::std::mutex _M_Cs;
	::Concurrency::scheduler_ptr _M_pScheduler;
	_TaskCollectionState _M_State;
};
static_assert(sizeof(::Concurrency::details::_TaskCollection_t) == sizeof(future_help), "");
#endif
template<typename T>
class future_state
{
protected:
	Concurrency::task<T> task_;
public:
	future_state() noexcept {}
	future_state(const future_state& other) = default;
	future_state(future_state&& other) noexcept
		: task_(std::move(other.task_))
	{
	}
	future_state& operator=(const future_state& other) = default;
	future_state& operator=(future_state&& other) noexcept
	{
		if (this == &other)
			return *this;
		task_ = std::move(other.task_);
		return *this;
	}
	future_state(const Concurrency::task<T>& t)
		:task_(t)
	{

	}
	~future_state() = default;
public:
	operator Concurrency::task<T>() const
	{
		return task_;
	}

	bool is_ready() const
	{
		if (!valid())
		{
			throw future_error(future_errc::no_state);
		}
		return task_.is_done();
	}
	template<typename Fn>
	auto then(Fn&& fn)->future<typename Concurrency::details::_FunctionTypeTraits<Fn,T>::_FuncRetType>
	{
		return task_.then(std::forward<Fn>(fn));
	}
	bool valid() const noexcept
	{
		return task_._GetImpl() != nullptr;
	}
	void wait() const
	{
		if (!valid())
		{
			throw future_error(future_errc::no_state);
		}
		task_.wait();
	}
	template< class Rep, class Period >
	future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const
	{
		if (!valid())
		{
			throw future_error(future_errc::no_state);
		}
		if (!task_._GetImpl())
			return (future_status::deferred);

		auto help = reinterpret_cast<future_help*>(std::addressof(task_._GetImpl()->_M_TaskCollection));
#if _MSC_VER < 1900
		stdext::threads::xtime _Tgt = std::_To_xtime(timeout_duration);
		xtime now;
		xtime_get(&now, TIME_UTC);
		if (get_future_help()->_M_Completed.wait(_Xtime_diff_to_millis2(&_Tgt, &now)) == 0)
#else
		std::unique_lock<std::mutex> lock(help->_M_Cs);
		if (help->_M_StateChanged.wait_for(lock, timeout_duration, [this] {return this->is_ready(); }))
#endif
			return (future_status::ready);
		return (future_status::timeout);
	}
	template< class Clock, class Duration >
	future_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const
	{
		if (!valid())
		{
			throw future_error(future_errc::no_state);
		}
		typename std::chrono::time_point<Clock, Duration>::duration
			_Rel_time = timeout_time - Clock::now();
		return wait_for(_Rel_time);
	}

};

template<typename T>
class future : public future_state<T>
{
	using base_type = future_state<T>;
public:
	future() noexcept {}

	future(future&& other) noexcept
		: future_state<T>(std::move(other))
	{

	}
	future(const Concurrency::task<T>& task)
		: future_state<T>(task)
	{
		
	}
	future(const future& other) = delete;
	~future() = default;
	future& operator=(const future&) = delete;
	future& operator=(future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<T>::operator =(std::move(other));
		return *this;
	}
	shared_future<T> share() noexcept
	{
		return this->task_;
	}
	T get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		return this->task_.get();
	}
};
template<typename T>
class future<T&> : public future_state<T*>
{
	using base_type = future_state<T*>;
public:
	future() noexcept = default;

	future(future&& other) noexcept
		: future_state<T*>(std::move(other))
	{

	}
	future(const Concurrency::task<T*>& task)
		: future_state<T*>(task)
	{

	}
	future(const future& other) = delete;
	~future() = default;
	future& operator=(const future&) = delete;
	future& operator=(future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<T*>::operator =(std::move(other));
		return *this;
	}
	shared_future<T&> share() noexcept
	{
		return this->task_;
	}
	T& get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		return *this->task_.get();
	}
};
template<>
class future<void> : public future_state<void>
{
	using base_type = future_state<void>;
public:
	future() noexcept = default;

	future(future&& other) noexcept
		: future_state<void>(std::move(other))
	{
		
	}
	future(const Concurrency::task<void>& task)
		: future_state<void>(task)
	{

	}
	future(const future& other) = delete;
	~future() = default;
	future& operator=(const future&) = delete;
	future& operator=(future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<void>::operator =(std::move(other));
		return *this;
	}

	shared_future<void> share() noexcept;

	void get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		this->task_.get();
	}
};

template<typename T>
class shared_future : public future_state<T>
{
	using base_type = future_state<T>;
public:
	shared_future() noexcept = default;
	shared_future(const Concurrency::task<T>& t)
		:base_type(t)
	{

	}
	shared_future(shared_future&& other) noexcept
		: future_state<T>(std::move(other))
	{

	}
	shared_future(const shared_future& other) = default;
	~shared_future() = default;
	shared_future& operator=(const shared_future&) = default;
	shared_future& operator=(shared_future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<T>::operator =(std::move(other));
		return *this;
	}
	T get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		return this->task_.get();
	}
};
template<typename T>
class shared_future<T&> : public future_state<T*>
{
	using base_type = future_state<T*>;
public:
	shared_future() noexcept = default;

	shared_future(shared_future&& other) noexcept
		: future_state<T*>(std::move(other))
	{

	}
	shared_future(const Concurrency::task<T*>& t)
		:base_type(t)
	{

	}
	shared_future(const shared_future& other) = default;
	~shared_future() = default;
	shared_future& operator=(const shared_future&) = default;
	shared_future& operator=(shared_future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<T*>::operator =(std::move(other));
		return *this;
	}
	T& get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		return *this->task_.get();
	}
};
template<>
class shared_future<void> : public future_state<void>
{
	using base_type = future_state<void>;
public:
	shared_future() noexcept = default;
	shared_future(const Concurrency::task<void>& t)
		:base_type(t)
	{

	}
	shared_future(shared_future&& other) noexcept
		: future_state<void>(std::move(other))
	{

	}
	shared_future(const shared_future& other) = default;
	~shared_future() = default;
	shared_future& operator=(const shared_future&) = default;
	shared_future& operator=(shared_future&& other) noexcept
	{
		if (this == &other)
			return *this;
		future_state<void>::operator =(std::move(other));
		return *this;
	}
	void get()
	{
		if (!base_type::valid())
		{
			throw future_error(future_errc::no_state);
		}
		this->task_.get();
	}
};

inline shared_future<void> future<void>::share() noexcept
{
	return this->task_;
}
struct thread_exit_help
{
	std::condition_variable cv_;
	std::mutex mtx_;
};
template<class T>
class promise_state_base
{
protected:
	std::atomic<thread_exit_help*> threadExitHelp_;
	Concurrency::task_completion_event<T> event_;
	Concurrency::task<T> task_;
public:
	promise_state_base()
		:task_(event_)
	{

	}

	promise_state_base(const promise_state_base& other) = delete;

	promise_state_base(promise_state_base&& other) noexcept
		:
		event_(std::move(other.event_)),
		task_(std::move(other.task_))
	{
		threadExitHelp_.exchange(other.threadExitHelp_);
		other.threadExitHelp_.exchange(nullptr);
	}

	promise_state_base& operator=(const promise_state_base& other) = delete;

	promise_state_base& operator=(promise_state_base&& other) noexcept
	{
		if (this == &other)
			return *this;
		threadExitHelp_.exchange(other.threadExitHelp_);
		other.threadExitHelp_.exchange(nullptr);
		event_ = std::move(other.event_);
		task_ = std::move(other.task_);
		return *this;
	}

	~promise_state_base() noexcept
	{	// destroy
		if (task_._GetImpl() != nullptr && !task_.is_done())
		{
			future_error error(make_error_code(future_errc::broken_promise));
			event_.set_exception(std::make_exception_ptr(error));
		}
		auto p = threadExitHelp_.load(std::memory_order_acquire);
		if (p != nullptr)
		{
			delete p;
		}
	}
	Concurrency::task<T> get_task() const
	{
		return task_;
	}

	
	void set_exception(std::exception_ptr e)
	{
		event_.set_exception(e);
	}

	void set_exception_at_thread_exit(std::exception_ptr e)
	{
		notify_at_thread_exit();
		event_.set_exception(e);
	}
protected:
	void notify_at_thread_exit()
	{
		auto now = threadExitHelp_.load(std::memory_order_acquire);
		if (now == nullptr)
		{
			auto ptr = std::make_unique<thread_exit_help>();
			now = threadExitHelp_.load(std::memory_order_acquire);
			if (now == nullptr)
			{
				if (threadExitHelp_.compare_exchange_strong(now, ptr.get(), std::memory_order_acq_rel))
				{
					ptr.release();
					std::notify_all_at_thread_exit(now->cv_, std::unique_lock<std::mutex>(now->mtx_));
				}
			}
		}
		throw make_error_code(future_errc::promise_already_satisfied);
	}
};

template<class T>
class promise_state : public promise_state_base<T>
{
public:
	promise_state() = default;

	promise_state(const promise_state& other) = delete;

	promise_state(promise_state&& other) noexcept
		: promise_state_base<T>(std::move(other))
	{
	}

	promise_state& operator=(const promise_state& other) = delete;

	promise_state& operator=(promise_state&& other) noexcept
	{
		if (this == &other)
			return *this;
		promise_state_base<T>::operator =(std::move(other));
		return *this;
	}

	~promise_state() = default;
	void set_value(const T& v)
	{
		event_.set(v);
	}
	void set_value_at_thread_exit(const T& v)
	{
		notify_at_thread_exit();
		event_.set(v);
	}
	void set_value(T&& v)
	{
		event_.set(v);
	}
	void set_value_at_thread_exit(T&& v)
	{
		notify_at_thread_exit();
		event_.set(v);
	}
};
template<>
class promise_state<void> : public promise_state_base<void>
{
public:
	promise_state() = default;

	promise_state(const promise_state& other) = delete;

	promise_state(promise_state&& other) noexcept
		: promise_state_base<void>(std::move(other))
	{
	}

	promise_state& operator=(const promise_state& other) = delete;

	promise_state& operator=(promise_state&& other) noexcept
	{
		if (this == &other)
			return *this;
		promise_state_base<void>::operator =(std::move(other));
		return *this;
	}
	~promise_state() = default;
	void set_value()
	{
		event_.set();
	}

	void set_value_at_thread_exit()
	{
		notify_at_thread_exit();
		event_.set();
	}
};

struct allocator_arg_t {};

template<class T>
class promise
{
public:
	promise()
		: promiseState_(
			std::make_shared<promise_state<T>>())
	{

	}
	~promise() = default;
	template<class _Alloc>
	promise(allocator_arg_t, const _Alloc& _Al)
		: promiseState_(
			std::allocate_shared<promise_state<T>>(_Al))
	{	// construct with allocator
	}

	promise(promise&& other) _NOEXCEPT
		: promiseState_(std::move(other.promiseState_))
	{	// construct from rvalue promise object
	}

	promise& operator=(promise&& other) _NOEXCEPT
	{	// assign from rvalue promise object
		promiseState_ = std::move(other.promiseState_);
		return (*this);
	}
	void swap(promise& other) _NOEXCEPT
	{	// exchange with other
		std::swap(promiseState_, other.promiseState_);
	}

	future<T> get_future()
	{
		return promiseState_->get_task();
	}

	void set_value(const T& v)
	{
		promiseState_->set_value(v);
	}

	void set_value_at_thread_exit(const T& v)
	{
		promiseState_->set_value_at_thread_exit(v);
	}

	void set_value(T&& v)
	{
		promiseState_->set_value(v);
	}

	void set_value_at_thread_exit(T&& v)
	{
		promiseState_->set_value_at_thread_exit(v);
	}

	void set_exception(std::exception_ptr e)
	{
		promiseState_->set_exception(e);
	}

	void set_exception_at_thread_exit(std::exception_ptr e)
	{
		promiseState_->set_exception_at_thread_exit(e);
	}
public:
	promise(const promise&) = delete;
	promise& operator=(const promise&) = delete;
private:
	std::shared_ptr<promise_state<T>> promiseState_;
};

template<class T>
class promise<T&>
{
public:
	promise()
		: promiseState_(
			std::make_shared<promise_state<T*>>())
	{

	}
	~promise() = default;
	template<class _Alloc>
	promise(allocator_arg_t, const _Alloc& _Al)
		: promiseState_(
			std::allocate_shared<promise_state<T*>>(_Al))
	{	// construct with allocator
	}

	promise(promise&& other) _NOEXCEPT
		: promiseState_(std::move(other.promiseState_))
	{	// construct from rvalue promise object
	}

	promise& operator=(promise&& other) _NOEXCEPT
	{	// assign from rvalue promise object
		promiseState_ = std::move(other.promiseState_);
		return (*this);
	}
	void swap(promise& other) _NOEXCEPT
	{	// exchange with other
		std::swap(promiseState_, other.promiseState_);
	}

	future<T&> get_future()
	{
		return promiseState_->get_task();
	}

	void set_value(T& v)
	{
		promiseState_->set_value(std::addressof(v));
	}

	void set_value_at_thread_exit(T& v)
	{
		promiseState_->set_value_at_thread_exit(std::addressof(v));
	}

	void set_exception(std::exception_ptr e)
	{
		promiseState_->set_exception(e);
	}

	void set_exception_at_thread_exit(std::exception_ptr e)
	{
		promiseState_->set_exception_at_thread_exit(e);
	}
public:
	promise(const promise&) = delete;
	promise& operator=(const promise&) = delete;
private:
	std::shared_ptr<promise_state<T*>> promiseState_;
};

template<>
class promise<void>
{
public:
	promise()
		: promiseState_(
			std::make_shared<promise_state<void>>())
	{

	}
	template<class _Alloc>
	promise(allocator_arg_t, const _Alloc& _Al)
		: promiseState_(
			std::allocate_shared<promise_state<T>>(_Al))
	{	// construct with allocator
	}

	promise(promise&& other) _NOEXCEPT
		: promiseState_(std::move(other.promiseState_))
	{	// construct from rvalue promise object
	}

	promise& operator=(promise&& other) _NOEXCEPT
	{	// assign from rvalue promise object
		promiseState_ = std::move(other.promiseState_);
		return (*this);
	}
	void swap(promise& other) _NOEXCEPT
	{	// exchange with other
		std::swap(promiseState_, other.promiseState_);
	}

	future<void> get_future()
	{
		return promiseState_->get_task();
	}

	void set_value()
	{
		promiseState_->set_value();
	}

	void set_value_at_thread_exit()
	{
		promiseState_->set_value_at_thread_exit();
	}

	void set_exception(std::exception_ptr e)
	{
		promiseState_->set_exception(e);
	}

	void set_exception_at_thread_exit(std::exception_ptr e)
	{
		promiseState_->set_exception_at_thread_exit(e);
	}
public:
	promise(const promise&) = delete;
	promise& operator=(const promise&) = delete;
private:
	std::shared_ptr<promise_state<void>> promiseState_;
};
template<class _Ty>
void swap(promise<_Ty>& _Left, promise<_Ty>& _Right) _NOEXCEPT
{	// exchange _Left and _Right
	_Left.swap(_Right);
}

template <class _Fret>
struct _P_arg_type
{	// type for function
	typedef _Fret type;
};

template <class _Fret>
struct _P_arg_type<_Fret&>
{	// type for ref to function
	typedef _Fret *type;
};

template <>
struct _P_arg_type<void>
{	// type for void function
	typedef int type;
};

template<class Fn, class Ret, class...ArgTypes >
std::add_pointer_t<std::remove_reference_t<Ret>> invoker_impl(std::true_type, Fn&& fn, ArgTypes... args)
{
	return std::addressof(fn(std::forward<ArgTypes>(args)...));
}
template<class Fn, class Ret, class...ArgTypes >
Ret invoker_impl(std::false_type, Fn&& fn, ArgTypes... args)
{
	return fn(std::forward<ArgTypes>(args)...);
}
template<class Fn, class Ret, class...ArgTypes, class Sfinae = std::is_lvalue_reference<Ret>>
auto invoker(Fn&& fn, ArgTypes... args)->decltype(invoker_impl(Sfinae{}, fn, std::forward<ArgTypes>(args)...))
{
	return invoker_impl(Sfinae{}, std::forward<Fn>(fn), std::forward<ArgTypes>(args)...);
}

template<class Ret,
	class... ArgTypes>
	class packaged_task<Ret(ArgTypes...)>
{	// class that defines an asynchronous provider that returns the
	// result of a call to a function object
	typedef typename _P_arg_type<Ret>::type future_value_type;
	typedef std::shared_ptr<promise_state<future_value_type>> promise_type;
public:
	packaged_task() noexcept = default;

	template<class Fn>
	explicit packaged_task(Fn&& fn)
		:fn_(std::forward<Fn>(fn)), promise_(std::make_shared<promise_state<future_value_type>>())
	{	// construct from rvalue function object

	}

	packaged_task(packaged_task&& other) noexcept
		: fn_(std::move(other.fn_))
		, promise_(std::move(other.promise_))
	{	// construct from rvalue packaged_task object
	}

	packaged_task& operator=(packaged_task&& other) noexcept
	{	// assign from rvalue packaged_task object
		promise_ = _STD forward<promise_type>(other.promise_);
		fn_ = std::move(other.fn_);
		return (*this);
	}

	template<class Fn,
		class Alloc>
		explicit packaged_task(allocator_arg_t, const Alloc& al,
			Fn&& fn)
		:fn_(std::forward<Fn>(fn)), promise_(std::allocate_shared<promise_state<future_value_type>>(al))
	{	// construct from rvalue function object and allocator

	}

	~packaged_task() noexcept
	{	// destroy
	}

	void swap(packaged_task& other) noexcept
	{	// exchange with other
		_STD swap(promise_, other.promise_);
		_STD swap(fn_, other.fn_);
	}

	explicit operator bool() const noexcept	// retained
	{	// return status
		return valid();
	}

	bool valid() const
	{	// return status
		return promise_ != nullptr && fn_ != nullptr;
	}

	future<Ret> get_future()
	{	// return a future object that shares the associated
		// asynchronous state
		return promise_->get_future();
	}

	void operator()(ArgTypes... args)
	{	// call the function object
		if (promise_->get_task().is_done())
			_Throw_future_error(
				make_error_code(future_errc::promise_already_satisfied));
		try
		{
			promise_->set_value(invoker<Ret>(fn_, _STD forward<ArgTypes>(args)...));
		}
		catch (...)
		{
			promise_->set_exception(std::current_exception());
		}
		
	}

	void make_ready_at_thread_exit(ArgTypes... args)
	{	// call the function object and block until thread exit
		if (promise_->get_task().is_done())
			_Throw_future_error(
				make_error_code(future_errc::promise_already_satisfied));
		try
		{
			promise_->set_value_at_thread_exit(invoker<Ret>(fn_, _STD forward<ArgTypes>(args)...));
		}
		catch (...)
		{
			promise_->set_value_at_thread_exit(std::current_exception());
		}
	}

	void reset()
	{	// reset to newly constructed state
		promise_ = std::make_shared<promise_type>();
	}

private:
	std::function<Ret(ArgTypes...)> fn_;
	promise_type promise_;
public:
	packaged_task(const packaged_task&) = delete;
	packaged_task& operator=(const packaged_task&) = delete;
};
enum class launch {	// names for launch options passed to async
	async = 0x1,
	deferred = 0x2,
	any = async | deferred,	// retained
	sync = deferred
};

inline launch operator&(launch _Left, launch _Right)
{	/* return _Left&_Right */
	return (static_cast<launch>(static_cast<unsigned int>(_Left)
		& static_cast<unsigned int>(_Right)));
}

inline launch operator|(launch _Left, launch _Right)
{	/* return _Left|_Right */
	return (static_cast<launch>(static_cast<unsigned int>(_Left)
		| static_cast<unsigned int>(_Right)));
}

inline launch operator^(launch _Left, launch _Right)
{	/* return _Left^_Right */
	return (static_cast<launch>(static_cast<unsigned int>(_Left)
		^ static_cast<unsigned int>(_Right)));
}

inline launch operator~(launch _Left)
{	/* return ~_Left */
	return (static_cast<launch>(~static_cast<unsigned int>(_Left)));
}

inline launch& operator&=(launch& _Left, launch _Right)
{	/* return _Left&=_Right */
	_Left = _Left & _Right;
	return (_Left);
}

inline launch& operator|=(launch& _Left, launch _Right)
{	/* return _Left|=_Right */
	_Left = _Left | _Right;
	return (_Left);
}

inline launch& operator^=(launch& _Left, launch _Right)
{	/* return _Left^=_Right */
	_Left = _Left ^ _Right;
	return (_Left);
}
template<class T>
struct is_launch_type
	: std::false_type
{	// tests for _Launch_type argument
};

template<>
struct is_launch_type<launch>
	: std::true_type
{	// tests for _Launch_type argument
};

// TEMPLATE FUNCTION _Async
template<class _Fty,
	class... _ArgTypes> inline
	future<typename std::result_of<_Fty(_ArgTypes...)>::type>
	_Async(launch _Policy, _Fty&& _Fnarg,
		_ArgTypes&&... _Args)
{	// return a future object whose associated asynchronous state
	// manages a function object
	typedef typename std::result_of<_Fty(_ArgTypes...)>::type _Ret;
	typedef typename _P_arg_type<_Ret>::type _Ptype;
	// not support defferd 
	(void)_Policy;
	Concurrency::task<_Ptype> task([=] {return invoker<_Ret>(_Fnarg, std::forward<_ArgTypes>(_Args)...); });
	return task;
}
template<class Fn,
	class... ArgTypes> inline
	future<typename std::result_of<
	typename std::enable_if<!is_launch_type<
	typename std::decay<Fn>::type>::value, Fn>
	::type(ArgTypes...)>::type>
	async(Fn&& fn, ArgTypes&&... args)
{	// return a future object whose associated asynchronous state
	// manages a function object
	return (_Async(launch::any, _Decay_copy(_STD forward<Fn>(fn)),
		_Decay_copy(_STD forward<ArgTypes>(args))...));
}

template<class PolicyType,
	class Fn,
	class... ArgTypes> inline
	future<typename std::result_of<
	typename std::enable_if<is_launch_type<
	PolicyType>::value, Fn>
	::type(ArgTypes...)>::type>
	async(PolicyType policy, Fn&& fn,
		ArgTypes&&... args)
{	// return a future object whose associated asynchronous state
	// manages a function object
	return (_Async(policy, _Decay_copy(_STD forward<Fn>(fn)),
		_Decay_copy(_STD forward<ArgTypes>(args))...));
}


template<typename T>
future<std::decay_t<T>> make_ready_future(T&& value)
{
	promise<std::decay_t<T>> promise;
	promise.set_value(std::forward<T>(value));
	return promise.get_future();
}
inline future<void> make_ready_future()
{
	promise<void> promise;
	promise.set_value();
	return promise.get_future();
}
template<typename T>
future<T> make_exceptional_future(std::exception_ptr ex)
{
	promise<std::decay_t<T>> promise;
	promise.set_exception(ex);
	return promise.get_future();
}
template<typename T, typename E>
inline future<void> make_exceptional_future(E ex)
{
	promise<void> promise;
	promise.set_exception(std::make_exception_ptr(ex));
	return promise.get_future();
}

namespace detail
{
template<typename T>
future<T> get_future_impl(future<T>& f)
{
	return static_cast<Concurrency::task<T>>(f);
}
template<typename T>
future<T> get_future_impl(promise<T>& f)
{
	return f.get_future();
}
template<typename T>
future<T> get_future_impl(packaged_task<T>& f)
{
	return f.get_future();
}
template<typename T>
future<T> copy_future(future<T>& f)
{
	return get_future_impl(f);
}

template<typename Tuple, typename Fn, size_t... Index >
void for_each_tuple_impl(Tuple&& tuple, Fn&& fn, std::index_sequence<Index...>)
{
	std::initializer_list<int>{(std::forward<Fn>(fn)(std::get<Index>(std::forward<Tuple>(tuple))), 0)...};
}

template<typename Fn, typename ... Types>
void for_each_tuple(std::tuple<Types...>& tuple, Fn&& fn)
{
	for_each_tuple_impl(tuple, std::forward<Fn>(fn), std::index_sequence_for<Types...>{});
}

template<typename Tuple, typename Fn, size_t... Index >
void for_each_tuple_with_index_impl(Tuple&& tuple, Fn&& fn, std::index_sequence<Index...>)
{
	std::initializer_list<int>{(std::forward<Fn>(fn)(std::get<Index>(std::forward<Tuple>(tuple)), Index), 0)...};
}

template<typename Fn, typename ... Types>
void for_each_tuple_with_index(std::tuple<Types...>& tuple, Fn&& fn)
{
	for_each_tuple_with_index_impl(tuple, std::forward<Fn>(fn), std::index_sequence_for<Types...>{});
}
}

template <class InputIt>
auto when_all(InputIt first, InputIt last)
->future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
{
	using future_type = typename std::iterator_traits<InputIt>::value_type;
	concurrency::when_all();
	struct WhenAllHelper : std::enable_shared_from_this<WhenAllHelper>
	{
		using futures_type = std::vector<future_type>;

		WhenAllHelper(size_t size)
		{
			futures_.reserve(size);
		}
		~WhenAllHelper()
		{
			promise_.set_value(std::move(futures_));
		}
		futures_type futures_;
		promise<futures_type> promise_;
	};
	auto helper = std::make_shared<WhenAllHelper>(std::distance(first, last));

	for (auto iter = first; iter != last; ++iter)
	{
		iter->then([=](future_type& f)
		{
			helper->futures_.emplace_back(detail::get_future_impl(f));
		});
	}
	return helper->promise_.get_future();
}

template<class Collection>
auto when_all(Collection&& c) -> decltype(when_all(c.begin(), c.end())) {
	return when_all(c.begin(), c.end());
}

template<class... Futures>
auto when_all(Futures&&... futures)
->future<std::tuple<std::decay_t<Futures>...>>
{
	struct WhenAllHelper : std::enable_shared_from_this<WhenAllHelper>
	{
		using futures_type = std::tuple<std::decay_t<Futures>...>;

		WhenAllHelper(Futures&&... futures)
			:futures_(detail::copy_future(std::forward<Futures>(futures))...)
		{
			count_ = 0;
		}

		~WhenAllHelper()
		{
			promise_.set_value(std::move(futures_));
		}
		size_t count_;
		futures_type futures_;
		promise<futures_type> promise_;
	};
	auto helper = std::make_shared<WhenAllHelper>(std::forward<Futures>(futures)...);
	detail::for_each_tuple(helper->futures_, [helper](auto& f)
	{
		f.then([=] {
			++helper->count_;
		});
	});
	return helper->promise_.get_future();
}

template<class Sequence >
struct when_any_result
{
	using sequence_type = Sequence;
	std::size_t index;
	Sequence futures;
};

template<class InputIt>
auto when_any(InputIt first, InputIt last)
->future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
{
	using future_type = typename std::iterator_traits<InputIt>::value_type;

	struct WhenAllAny : std::enable_shared_from_this<WhenAllAny>
	{
		using result = when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>;

		WhenAllAny(size_t size)
			:has_index_(false)
		{
			result_.futures.reserve(size);
		}
		void notify(size_t index)
		{
			if (!has_index_)
			{
				// 如果有人锁了,说明不需要在继续了
				std::unique_lock<std::mutex> lock(mtx_, std::try_to_lock);
				if (lock && !has_index_)
				{
					has_index_ = true;
					result_.index = index;
					promise_.set_value(std::move(result_));
				}
			}
		}
		std::atomic_bool has_index_;
		std::mutex mtx_;
		result result_;
		promise<result> promise_;
	};

	auto helper = std::make_shared<WhenAllAny>(std::distance(first, last));

	for (auto iter = first; iter != last; ++iter)
	{
		auto index = std::distance(iter, last);
		helper->result_.futures.emplace_back(detail::get_future_impl(*iter));
		iter->then([=]{
			helper->notify(index);
		});
	}
	return helper->promise_.get_future();
}

template<class Collection>
auto when_any(Collection&& c) -> decltype(when_any(c.begin(), c.end()))
{
	return when_any(c.begin(), c.end());
}

template<class... Futures>
auto when_any(Futures&&... futures)
->future<when_any_result<std::tuple<std::decay_t<Futures>...>>>
{
	struct WhenAnyHelper : std::enable_shared_from_this<WhenAnyHelper>
	{
		using result = when_any_result<std::tuple<std::decay_t<Futures>...>>;

		WhenAnyHelper(Futures&&... futures)
			:has_index_(false), result_{ 0, typename result::sequence_type(detail::copy_future(std::forward<Futures>(futures))...) }
		{
		}

		void notify(size_t index)
		{
			if (!has_index_)
			{
				// 如果有人锁了,说明不需要在继续了
				std::unique_lock<std::mutex> lock(mtx_, std::try_to_lock);
				if (lock && !has_index_)
				{
					has_index_ = true;
					result_.index = index;
					promise_.set_value(std::move(result_));
				}
			}
		}
		std::atomic_bool has_index_;
		std::mutex mtx_;
		result result_;
		promise<result> promise_;
	};
	auto helper = std::make_shared<WhenAnyHelper>(std::forward<Futures>(futures)...);
	detail::for_each_tuple_with_index(helper->result_.futures, [helper](auto& f, size_t index)
	{
		f.then([=] {
			helper->notify(index);
		});
	});
	return helper->promise_.get_future();
}

}
