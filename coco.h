#pragma once

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

namespace sch = std::chrono;

namespace coco
{
	using clock_t = sch::high_resolution_clock;

	struct nanoseconds
	{
		using type = sch::nanoseconds;
		static constexpr const char* name = "nanoseconds";
	};

	struct microseconds
	{
		using type = sch::microseconds;
		static constexpr const char* name = "microseconds";
	};

	struct milliseconds
	{
		using type = sch::milliseconds;
		static constexpr const char* name = "milliseconds";
	};

	struct seconds
	{
		using type = sch::seconds;
		static constexpr const char* name = "seconds";
	};

	struct minutes
	{
		using type = sch::minutes;
		static constexpr const char* name = "minutes";
	};

	struct hours
	{
		using type = sch::hours;
		static constexpr const char* name = "hours";
	};

	struct profile_result
	{
		std::string name;
		long long start, end;
		size_t threadID;
	};

	struct instrumentation_session
	{
		std::string m_name;
	};

	class instrumentor
	{
	public:
		instrumentor() : m_current_session(nullptr), m_profile_count(0), m_active(false) {}

		void begin_session(const std::string& name, const std::string& filepath = "results.json")
		{
			m_output_stream.open(filepath);
			write_header();
			m_current_session = new instrumentation_session{ name };
			m_active = true;
		}

		void end_session()
		{
			write_footer();
			m_output_stream.close();
			delete m_current_session;
			m_current_session = nullptr;
			m_profile_count = 0;
			m_active = false;
		}

		void write_profile(const profile_result& result)
		{
			if (m_profile_count++ > 0)
				m_output_stream << ",";

			std::string name = result.name;
			std::replace(name.begin(), name.end(), '"', '\'');

			m_output_stream << "{\"cat\":\"function\",";
			m_output_stream << "\"dur\":" << (result.end - result.start) << ',';
			m_output_stream << "\"name\":\"" << name << "\",";
			m_output_stream << "\"ph\":\"X\",";
			m_output_stream << "\"pid\":0,";
			m_output_stream << "\"tid\":" << result.threadID << ",";
			m_output_stream << "\"ts\":" << result.start << "}";

			m_output_stream.flush();
		}

		void write_header()
		{
			m_output_stream << "{\"otherData\": {},\"traceEvents\":[";
			m_output_stream.flush();
		}

		void write_footer()
		{
			m_output_stream << "]}";
			m_output_stream.flush();
		}

		static instrumentor& get()
		{
			static instrumentor instance;
			return instance;
		}

	private:
		instrumentation_session* m_current_session;
		std::ofstream m_output_stream;
		int m_profile_count;
		bool m_active;
	};

#ifdef __cpp_concepts
	namespace detail
	{
		template <class dur>
		concept duration_type = std::_Is_any_of_v<dur, coco::nanoseconds, coco::microseconds, coco::milliseconds, coco::seconds, coco::minutes, coco::hours>;
	}

#define _COCO_ENABLE_IF_DURATION_T(dur)
#define _COCO_CONCEPT_DURATION_T detail::duration_type
#else // __cpp_concepts
#define _COCO_ENABLE_IF_DURATION_T(dur) , std::enable_if_t<std::_Is_any_of_v<dur, coco::nanoseconds, coco::microseconds, coco::milliseconds, coco::seconds, coco::minutes, coco::hours>, int> = 0
#define _COCO_CONCEPT_DURATION_T class
#endif // __cpp_concepts

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
	class timer
	{
	public:
		timer(const std::string& name = std::string{ "Coco Timer" }) : m_name(name)
		{
			start();
		}

		~timer()
		{
			stop();
		}

		void start()
		{
			m_time = 0;
			m_paused = false;
			m_stopped = false;
			m_timepoint = now();
		}

		void pause()
		{
			if (!m_paused)
			{
				m_paused = true;
				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(now()).time_since_epoch().count();
				m_time += (end - start);
			}
		}

		void resume()
		{
			if (m_paused)
			{
				m_paused = false;
				m_timepoint = now();
			}
		}

		void reset()
		{
			m_time		= 0;
			m_paused	= false;
			m_stopped	= false;
			m_timepoint = now();
		}

		void stop()
		{
			if (!m_stopped)
			{
				m_stopped = true;
				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end	= tp_cast(now()).time_since_epoch().count();

				m_time += (end - start);
				if (m_print_when_stopped)
					std::cout << m_name << " : " << m_time << ' ' << _Duration::name << "\n";
			}
		}

		bool complated_on_time(long long time) const noexcept
		{
			if (!m_stopped)
				return false;
			return time >= m_time;
		}

		void set_print_state(bool state)
		{
			m_print_when_stopped = state;
		}

		bool is_printing() const noexcept
		{
			return m_print_when_stopped;
		}

		long long get_time() const
		{
			return m_time;
		}

	private:
		sch::time_point<clock_t> now()
		{
			return clock_t::now();
		}

		constexpr sch::time_point<clock_t, typename _Duration::type> tp_cast(const sch::time_point<clock_t>& tp)
		{
			return sch::time_point_cast<typename _Duration::type>(tp);
		}

		sch::time_point<clock_t> m_timepoint;
		std::string m_name;
		long long m_time			= 0;
		bool m_stopped				= false;
		bool m_paused				= false;
		bool m_print_when_stopped	= true;
	};

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
	class instrumentation_timer
	{
	public:
		instrumentation_timer(const std::string& name = std::string{ "Coco Instrumentation Timer" }) : m_name(name)
		{
			start();
		}

		~instrumentation_timer()
		{
			stop();
		}

		void start()
		{
			m_time = 0;
			m_stopped = false;
			m_timepoint = now();
		}

		void reset()
		{
			m_time = 0;
			m_stopped = false;
			m_timepoint = now();
		}

		void stop()
		{
			if (!m_stopped)
			{
				m_stopped = true;
				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(now()).time_since_epoch().count();

				m_time += (end - start);
				instrumentor::get().write_profile({ m_name, start, end, std::hash<std::thread::id>{}(std::this_thread::get_id()) });
			}
		}

		long long get_time() const
		{
			return m_time;
		}

	private:
		sch::time_point<clock_t> now()
		{
			return clock_t::now();
		}

		constexpr sch::time_point<clock_t, typename _Duration::type> tp_cast(const sch::time_point<clock_t>& tp)
		{
			return sch::time_point_cast<typename _Duration::type>(tp);
		}

		sch::time_point<clock_t> m_timepoint;
		std::string m_name;
		long long m_time = 0;
		bool m_stopped = false;
	};
}


#define _COCO_CONCAT_H(x, y)	x##y
#define _COCO_CONCAT(x, y)		_COCO_CONCAT_H(x,y)
#define _COCO_ADD_COUNTER(x)	_COCO_CONCAT(x, __COUNTER__)

//#define COCO_NO_PROFILE

#ifndef COCO_NO_PROFILE
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define _COCO_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__FUNCSIG__)
#define _COCO_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define _COCO_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define _COCO_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define _COCO_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define _COCO_FUNC_SIG __func__
#else
#define _COCO_FUNC_SIG "_COCO_FUNC_SIG unknown!"
#endif

#define COCO_SCOPE_TIMER()				coco::timer<coco::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)
#define COCO_SCOPE_TIMER_NAMED(name)	coco::timer<coco::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)(name)

#define COCO_PROFILE_BEGIN_SESSION(name, filepath)	coco::instrumentor::get().begin_session(name, filepath)
#define COCO_PROFILE_END_SESSION()					coco::instrumentor::get().end_session()
#define COCO_PROFILE_SCOPE(name)					coco::instrumentation_timer _COCO_ADD_COUNTER(timer)(name)
#define COCO_PROFILE_FUNCTION()						COCO_PROFILE_SCOPE(_COCO_FUNC_SIG)
#else // COCO_NO_PROFILE
#define COCO_PROFILE_BEGIN_SESSION(name, filepath)
#define COCO_PROFILE_END_SESSION()
#define COCO_PROFILE_SCOPE(name)
#define COCO_PROFILE_FUNCTION()
#endif  // COCO_NO_PROFILE

#undef _COCO_ENABLE_IF_DURATION_T
#undef _COCO_CONCEPT_DURATION_T
