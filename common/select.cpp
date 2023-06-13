#include "common/selectable.h"
#include "common/logger.h"
#include "common/select.h"
#include <stdio.h>
#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>


using namespace std;

namespace swss {

Select::Select()
{
    m_epoll_fd = ::epoll_create1(0);
    sz_selectables = 0;
    if (m_epoll_fd == -1)
    {
        std::string error = std::string("Select::constructor:epoll_create1: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }
}

Select::~Select()
{
    (void)::close(m_epoll_fd);
}

void Select::addSelectable(Selectable *selectable)
{
    const int fd = selectable->getFd();
    if(m_objects.find(fd) != m_objects.end())
    {
        SWSS_LOG_WARN("Selectable is already added to the list, ignoring.");
        return;
    }

    m_objects.emplace(std::piecewise_construct, 
                      std::forward_as_tuple(fd),
                      std::forward_as_tuple(selectable, selectable->getMaxEvents()));

    if (selectable->initializedWithData())
    {
        m_ready.insert(selectable);
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = { .fd = fd, },
    };

    int res = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (res == -1)
    {
        std::string error = std::string("Select::add_fd:epoll_ctl: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }

    sz_selectables += selectable->getMaxEvents();
}

void Select::removeSelectable(Selectable *selectable)
{
    const int fd = selectable->getFd();
    if (m_objects.find(fd) == m_objects.end())
    {   
        SWSS_LOG_ERROR("Selectable is not added to the list, ignoring..");
        return ;
    }

    auto events_sel = m_objects[fd].maxevents;    
    m_objects.erase(fd);
    m_ready.erase(selectable);
    sz_selectables = sz_selectables - events_sel ? events_sel <= sz_selectables : recompute_maxevents();

    int res = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (res == -1)
    {
        std::string error = std::string("Select::del_fd:epoll_ctl: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }
}

void Select::addSelectables(vector<Selectable *> selectables)
{
    for(auto it : selectables)
    {
        addSelectable(it);
    }
}

int Select::poll_descriptors(Selectable **c, unsigned int timeout, bool interrupt_on_signal = false)
{
    std::vector<struct epoll_event> events(sz_selectables);
    int ret;

    while(true)
    {
        ret = ::epoll_wait(m_epoll_fd, events.data(), static_cast<int>(sz_selectables), timeout);
        // on signal interrupt check if we need to return
        if (ret == -1 && errno == EINTR)
        {
            if (interrupt_on_signal)
            {
                return Select::SIGNALINT;
            }
        }
        // on all other errors break the loop
        else
        {
            break;
        }
    }

    if (ret < 0)
    {
        SWSS_LOG_WARN("epoll_wait returned %d, with error %s", ret, strerror(errno));
        return Select::ERROR;
    }

    for (int i = 0; i < ret; ++i)
    {
        int fd = events[i].data.fd;
        Selectable* sel = m_objects[fd].sel;
        try
        {
            sel->readData();
        }
        catch (const std::runtime_error& ex)
        {
            SWSS_LOG_ERROR("readData error: %s", ex.what());
            return Select::ERROR;
        }
        m_ready.insert(sel);
    }

    while (!m_ready.empty())
    {
        auto sel = *m_ready.begin();

        m_ready.erase(sel);
        // we must update clock only when the selector out of the m_ready
        // otherwise we break invariant of the m_ready
        sel->updateLastUsedTime();

        if (!sel->hasData())
        {
            continue;
        }

        *c = sel;

        if (sel->hasCachedData())
        {
            // reinsert Selectable back to the m_ready set, when there're more messages in the cache
            m_ready.insert(sel);
        }

        sel->updateAfterRead();

        return Select::OBJECT;
    }

    return Select::TIMEOUT;
}

int Select::select(Selectable **c, int timeout, bool interrupt_on_signal)
{
    SWSS_LOG_ENTER();

    int ret;

    *c = NULL;

    /* check if we have some data */
    ret = poll_descriptors(c, 0);

    /* return if we have data, we have an error or desired timeout was 0 */
    if (ret != Select::TIMEOUT || timeout == 0)
        return ret;

    /* wait for data */
    ret = poll_descriptors(c, timeout, interrupt_on_signal);

    return ret;

}

bool Select::isQueueEmpty()
{
    return m_ready.empty();
}

std::string Select::resultToString(int result)
{
    SWSS_LOG_ENTER();

    switch (result)
    {
        case swss::Select::OBJECT:
            return "OBJECT";

        case swss::Select::ERROR:
            return "ERROR";

        case swss::Select::TIMEOUT:
            return "TIMEOUT";

        case swss::Select::SIGNALINT:
            return "SIGNALINT";

        default:
            SWSS_LOG_WARN("unknown select result: %d", result);
            return "UNKNOWN";
    }
}

};
