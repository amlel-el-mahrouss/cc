/***
	(C) 2025 Amlal El Mahrouss
 */

#include <iostream>
#include <unordered_map>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdint.h>

namespace LibDebugger
{
	/// \brief Debugger interface class in C++
	/// \author Amlal El Mahrouss
	class IDebugger final
	{
	public:
		IDebugger()	 = default;
		~IDebugger() = default;

		IDebugger& operator=(const IDebugger&) = default;
		IDebugger(const IDebugger&)			   = default;

	public:
		void Attach(pid_t pid)
		{
			this->m_pid = pid;

			if (ptrace(PTRACE_ATTACH, this->m_pid, nullptr, nullptr) == -1)
			{
				perror("dbg: Attach");
				return;
			}

			waitpid(m_pid, nullptr, 0);

			std::cout << "[+] Attached to process: " << m_pid << std::endl;
		}

		void SetBreakpoint(void* addr)
		{
			long original_data = ptrace(PTRACE_PEEKTEXT, m_pid, addr, nullptr);
			if (original_data == -1)
			{
				perror("dbg: Peek");
			  return;
			}

			long data_with_int3 = (original_data & ~0xFF) | 0xCC; // Insert INT3 (0xCC)
			if (ptrace(PTRACE_POKETEXT, m_pid, addr, data_with_int3) == -1)
			{
				perror("dbg: Poke");
				return;
			}

			std::cout << "[+] Breakpoint set at: " << addr << std::endl;

			m_breakpoints[reinterpret_cast<uintptr_t>(addr)] = original_data; // Store original data
		}

		void ContinueExecution()
		{
			if (ptrace(PTRACE_CONT, m_pid, nullptr, nullptr) == -1)
			{
				perror("dbg: Cont");
        return;
			}

			int status;
			waitpid(m_pid, &status, 0);

			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)
			{
				std::cout << "[!] Breakpoint hit." << std::endl;
			}
		}

		void Detach()
		{
			if (ptrace(PTRACE_DETACH, m_pid, nullptr, nullptr) == -1)
			{
				perror("dbg: Detach");
				return;
			}

			std::cout << "[-] Detached from process: " << m_pid << std::endl;
		}

		std::unordered_map<uintptr_t, long>& Breakpoints()
		{
			return m_breakpoints;
		}

	private:
		pid_t								m_pid;
		std::unordered_map<uintptr_t, long> m_breakpoints;
	};
} // namespace LibDebugger
