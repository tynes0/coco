#pragma once

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <numeric>
#include <cassert>
#include <unordered_map>

namespace sch = std::chrono;

#ifdef _DEBUG
#define COCO_ASSERT(condition, message)																\
    do																								\
    {																								\
        if (!(condition))																			\
        {																							\
            std::cerr << "Assertion failed: " << #condition << " (" << message << ")" << std::endl; \
            std::cerr << "File: " << __FILE__ << ", Line: " << __LINE__ << std::endl;				\
            std::abort();																			\
        }																							\
    } while (false)
#endif // _DEBUG

namespace coco
{
	using clock_t = sch::high_resolution_clock;
	namespace time_units
	{
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
	}

#ifdef __cpp_concepts
	namespace detail
	{
		template <class dur>
		concept duration_type = std::_Is_any_of_v<dur, coco::time_units::nanoseconds, coco::time_units::microseconds, coco::time_units::milliseconds, coco::time_units::seconds, coco::time_units::minutes, coco::time_units::hours>;
	}

#define _COCO_ENABLE_IF_DURATION_T(dur)
#define _COCO_CONCEPT_DURATION_T detail::duration_type
#else // __cpp_concepts
#define _COCO_ENABLE_IF_DURATION_T(dur) , std::enable_if_t<std::_Is_any_of_v<dur, coco::time_units::nanoseconds, coco::time_units::microseconds, coco::time_units::milliseconds, coco::time_units::seconds, coco::time_units::minutes, coco::time_units::hours>, int> = 0
#define _COCO_CONCEPT_DURATION_T class
#endif // __cpp_concepts

	namespace detail
	{
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
	}

	class instrumentor
	{
	public:
		instrumentor() : m_current_session(nullptr), m_profile_count(0), m_active(false) {}

		void begin_session(const std::string& name, const std::string& filepath = "results.json")
		{
			m_output_stream.open(filepath);
			write_header();
			m_current_session = new detail::instrumentation_session{ name };
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

		void write_profile(const detail::profile_result& result)
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
		detail::instrumentation_session* m_current_session;
		std::ofstream m_output_stream;
		int m_profile_count;
		bool m_active;
	};

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
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
			if (m_stopped)
			{
				m_time = 0;
				m_paused = false;
				m_stopped = false;
				m_timepoint = now();
			}
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
			m_time = 0;
			m_paused = false;
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

		bool is_running() const noexcept
		{
			return !m_stopped;
		}

		bool is_paused() const noexcept
		{
			return m_paused;
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
		long long m_time = 0;
		bool m_stopped = true;
		bool m_paused = false;
		bool m_print_when_stopped = true;
	};

	class instrumentation_timer
	{
	public:
		instrumentation_timer(const std::string& name) : m_name(name)
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

		void stop()
		{
			if (!m_stopped)
			{
				auto end_timepoint = clock_t::now();

				long long start = tp_cast(m_timepoint).time_since_epoch().count();
				long long end = tp_cast(end_timepoint).time_since_epoch().count();

				m_time = end - start;

				size_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
				instrumentor::get().write_profile({ m_name, start, end, threadID });
				m_stopped = true;
			}
		}

		bool complated_on_time(long long time) const noexcept
		{
			if (!m_stopped)
				return false;
			return time >= m_time;
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

		constexpr sch::time_point<clock_t, typename time_units::microseconds::type> tp_cast(const sch::time_point<clock_t>& tp)
		{
			return sch::time_point_cast<typename time_units::microseconds::type>(tp);
		}

		sch::time_point<clock_t> m_timepoint;
		std::string m_name;
		long long m_time = 0;
		bool m_stopped = false;
	};

	class timer_statistics
	{
	public:
		void add_measurement(long long time)
		{
			m_measurements.push_back(time);
		}

		void clear_measurements()
		{
			m_measurements.clear();
		}

		double calculate_average() const
		{
			if (m_measurements.empty()) 
				return 0.0;
			long long sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0LL);
			return static_cast<double>(sum) / m_measurements.size();
		}

		double calculate_variance() const
		{
			if (m_measurements.empty()) return 0.0;
			double avg = calculate_average();
			double variance = 0.0;
			for (long long time : m_measurements)
			{
				variance += std::pow(time - avg, 2);
			}
			variance /= m_measurements.size();
			return variance;
		}

		double calculate_standard_deviation() const
		{
			return std::sqrt(calculate_variance());
		}

		double calculate_median() const
		{
			if (m_measurements.empty()) 
				return 0.0;
			std::vector<long long> sorted_measurements = m_measurements;
			std::sort(sorted_measurements.begin(), sorted_measurements.end());
			size_t n = sorted_measurements.size();
			if (n % 2 == 0)
				return static_cast<double>(sorted_measurements[n / 2 - 1] + sorted_measurements[n / 2]) / 2.0;
			else
				return static_cast<double>(sorted_measurements[n / 2]);
		}

		long long get_min_value() const
		{
			if (m_measurements.empty()) 
				return 0;
			return *std::min_element(m_measurements.begin(), m_measurements.end());
		}

		long long get_max_value() const
		{
			if (m_measurements.empty()) 
				return 0;
			return *std::max_element(m_measurements.begin(), m_measurements.end());
		}
	
	private:
		std::vector<long long> m_measurements;
	};

	class timer_data_logger
	{
	public:
		timer_data_logger() : m_stats(new timer_statistics()) {}

		timer_data_logger(const timer_statistics& stats) : m_stats(new timer_statistics(stats)) {}

		timer_data_logger(timer_statistics&& stats) noexcept : m_stats(new timer_statistics(std::move(stats))) {}

		~timer_data_logger()
		{
			delete m_stats;
		}

		timer_statistics* get_statistics() const
		{
			return m_stats;
		}

		timer_data_logger& operator=(const timer_data_logger& other)
		{
			if (this != &other)
			{
				delete m_stats;
				m_stats = new timer_statistics(*other.m_stats);
			}
			return *this;
		}

		timer_data_logger& operator=(timer_data_logger&& other) noexcept
		{
			if (this != &other)
			{
				delete m_stats;
				m_stats = other.m_stats;
				other.m_stats = nullptr;
			}
			return *this;
		}

		void add_measurement(long long time)
		{
			m_stats->add_measurement(time);
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void log_statistics(const std::string& filename)
		{
			std::ofstream file(filename);
			if (file.is_open())
			{
				file << "Statistics Summary:\n";
				file << "-------------------\n";
				file << "Average Time: " << m_stats->calculate_average() << ' ' << _Duration::name << "\n";
				file << "Variance: " << m_stats->calculate_variance() << ' ' << _Duration::name << "\n";
				file << "Standard Deviation: " << m_stats->calculate_standard_deviation() << ' ' << _Duration::name << "\n";
				file << "Median Time: " << m_stats->calculate_median() << ' ' << _Duration::name << "\n";
				file << "Minimum Time: " << m_stats->get_min_value() << ' ' << _Duration::name << "\n";
				file << "Maximum Time: " << m_stats->get_max_value() << ' ' << _Duration::name << "\n";
				file << "-------------------\n";
				file.close();
			}
			else
			{
				COCO_ASSERT(false, "Failed to open file for writing.");
			}
		}

	private:
		timer_statistics* m_stats;
	};

	template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
	class multiple_timer_manager 
	{
	public:
		multiple_timer_manager() {}

		~multiple_timer_manager()
		{
			for (auto& timer : m_timers)
				delete timer.second;
		}

		void add_and_start_timer(const std::string& timer_name) 
		{
			COCO_ASSERT(m_timers.find(timer_name) == m_timers.end(), "Timer already exists!");
			m_timers[timer_name] = new coco::timer<_Duration>();
			//m_timers[timer_name]->set_print_state(false);
		}

		void stop_timer(const std::string& timer_name) 
		{
			COCO_ASSERT(m_timers.find(timer_name) != m_timers.end(), "Timer not found!");
			m_timers[timer_name]->stop();
			m_data_logger.add_measurement(m_timers[timer_name]->get_time());
		}

		void reset_timer(const std::string& timer_name) 
		{
			COCO_ASSERT(m_timers.find(timer_name) != m_timers.end(), "Timer not found!");
			m_timers[timer_name]->reset();
		}

		void pause_timer(const std::string& timer_name)
		{
			COCO_ASSERT(m_timers.find(timer_name) != m_timers.end(), "Timer not found!");
			m_timers[timer_name]->pause();
		}

		void resume_timer(const std::string& timer_name)
		{
			COCO_ASSERT(m_timers.find(timer_name) != m_timers.end(), "Timer not found!");
			m_timers[timer_name]->resume();
		}

		void remove_timer(const std::string& timer_name)
		{
			auto it = m_timers.find(timer_name);
			COCO_ASSERT(it != m_timers.end(), "Timer not found!");
			m_timers.erase(it);
		}

		void reset_all_timers() 
		{
			for (auto& timer : m_timers)
				timer.second->reset();
		}

		void stop_all_timers() 
		{
			for (auto& timer : m_timers) 
				timer.second->stop();
		}

		coco::timer<_Duration>* get_timer(const std::string& timer_name)
		{
			auto it = m_timers.find(timer_name);
			COCO_ASSERT(it != m_timers.end(), "Timer not found");
			return m_timers.at(timer_name);
		}

		void log_statistics(const std::string& filename) 
		{
			m_data_logger.log_statistics<_Duration>(filename);
		}

		bool is_timer_running(const std::string& timer_name) const 
		{
			if (m_timers.find(timer_name) != m_timers.end())
				return !m_timers.at(timer_name)->is_running();
			return false;
		}

		long long get_elapsed_time(const std::string& timer_name) const
		{
			COCO_ASSERT(m_timers.find(timer_name) != m_timers.end(), "Timer not found!");
			return m_timers.at(timer_name)->get_time();
		}

		void rename_timer(const std::string& old_name, const std::string& new_name)
		{
			COCO_ASSERT(m_timers.find(new_name) == m_timers.end(), "New timer name already exists!");
			auto it = m_timers.find(old_name);
			COCO_ASSERT(it != m_timers.end(), "Timer not found!");

			m_timers[new_name] = std::move(it->second);
			m_timers.erase(it);
		}

	private:
		std::unordered_map<std::string, coco::timer<_Duration>*> m_timers;
		coco::timer_data_logger m_data_logger;
	};


	class timer_controller 
	{
	public:
		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void start_timer(coco::timer<_Duration>& timer)
		{
			timer.start();
		}
		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void stop_timer(coco::timer<_Duration>& timer)
		{
			timer.stop();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void reset_timer(coco::timer<_Duration>& timer)
		{
			timer.reset();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void pause_timer(coco::timer<_Duration>& timer)
		{
			timer.pause();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void resume_timer(coco::timer<_Duration>& timer)
		{
			timer.resume();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		bool is_timer_running(const coco::timer<_Duration>& timer) const
		{
			return timer.is_running();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		bool is_timer_paused(const coco::timer<_Duration>& timer) const
		{
			return timer.is_paused();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		long long get_timer_time(const coco::timer<_Duration>& timer) const
		{
			return timer.get_time();
		}

		template <_COCO_CONCEPT_DURATION_T _Duration = coco::time_units::microseconds _COCO_ENABLE_IF_DURATION_T(_Duration)>
		void set_timer_print_state(coco::timer<_Duration>& timer, bool state)
		{
			timer.set_print_state(state);
		}
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

// console
#define COCO_SCOPE_TIMER()				coco::timer<coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)
#define COCO_SCOPE_TIMER_NAMED(name)	coco::timer<coco::time_units::microseconds> _COCO_ADD_COUNTER(__coco_timer_var_)(name)

// json
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
