#include "BreakPointContainer.hpp"

namespace LEXInterop::ScriptDebugger
{
    bool BreakPointContainer::ContainsFunc(const char* funcPath)
    {
        return ContainsFunc(std::string(funcPath));
    }

    bool BreakPointContainer::ContainsFunc(const std::string& funcPath)
    {
        std::lock_guard lock(m_Mutex);
        return m_BreakpointMap.find(funcPath) != m_BreakpointMap.end();
    }

    bool BreakPointContainer::HasBreakPoint(const std::string& funcPath, unsigned short location)
    {
        std::lock_guard lock(m_Mutex);

        if (const auto pair = m_BreakpointMap.find(funcPath); pair != m_BreakpointMap.end())
        {
            return pair->second.find(location) != pair->second.end();
        }
        return false;
    }

    void BreakPointContainer::AddBreakPoint(const char* funcPath, unsigned short location)
    {
        AddBreakPoint(std::string(funcPath), location);
    }

    void BreakPointContainer::AddBreakPoint(const std::string& funcPath, unsigned short location)
    {
        std::lock_guard lock(m_Mutex);

        if (const auto pair = m_BreakpointMap.find(funcPath); pair != m_BreakpointMap.end())
        {
            auto& breakPoints = pair->second;
            breakPoints.insert(location);
        }
        else
        {
            m_BreakpointMap.emplace(funcPath, std::set{ location });
        }
    }

    bool BreakPointContainer::RemoveBreakPoint(const char* funcPath, unsigned short location)
    {
        return RemoveBreakPoint(std::string(funcPath), location);
    }

    bool BreakPointContainer::RemoveBreakPoint(const std::string& funcPath, unsigned short location)
    {
        std::lock_guard lock(m_Mutex);

        if (const auto iter = m_BreakpointMap.find(funcPath); iter != m_BreakpointMap.end())
        {
            auto& breakPoints = iter->second;
            if (breakPoints.erase(location) > 0)
            {
                if (breakPoints.empty())
                {
                    m_BreakpointMap.erase(iter);
                }
                return true;
            }
        }
        return false;
    }

    void BreakPointContainer::ClearBreakPoints()
    {
        std::lock_guard lock(m_Mutex);
        m_BreakpointMap.clear();
    }
}
