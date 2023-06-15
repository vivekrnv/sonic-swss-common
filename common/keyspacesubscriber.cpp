#include <string>
#include <memory>
#include <algorithm>
#include <hiredis/hiredis.h>
#include "logger.h"
#include "dbconnector.h"
#include "keyspacesubscriber.h"

namespace swss {

KeySpaceSubscriber::KeySpaceSubscriber(DBConnector *parentConn, int pri) : RedisSelect(pri)
{
    m_subscribe.reset(parentConn->newConnector(RedisSelect::SUBSCRIBE_TIMEOUT));
}

void KeySpaceSubscriber::setClientName(const std::string& name)
{
    if (m_subscribe && !name.empty())
    {
        m_subscribe->setClientName(name);
    }
}

void KeySpaceSubscriber::subscribe(const std::string &pattern)
{
    m_subscribe->subscribe(pattern);
    m_patterns.insert(pattern);
}

void KeySpaceSubscriber::psubscribe(const std::string &pattern)
{
    m_subscribe->psubscribe(pattern);
    m_patterns.insert(pattern);
}

void KeySpaceSubscriber::punsubscribe(const std::string &pattern)
{
    m_subscribe->punsubscribe(pattern);
    m_patterns.erase(pattern);
}

void KeySpaceSubscriber::subscribe(const std::vector<std::string>& patterns)
{
    m_subscribe->subscribe(processBulk(patterns, true));
}

void KeySpaceSubscriber::psubscribe(const std::vector<std::string>& patterns)
{
    m_subscribe->psubscribe(processBulk(patterns, true));
}

void KeySpaceSubscriber::punsubscribe(const std::vector<std::string>& patterns)
{
    m_subscribe->punsubscribe(processBulk(patterns, false));
}

std::string KeySpaceSubscriber::processBulk(const std::vector<std::string> &patterns, bool add)
{
    std::string pattern = "";
    int num_patterns = (int)patterns.size();
    for (auto i = 0; i < num_patterns; i++)
    {
        pattern += patterns[i];
        if (i != num_patterns-1)
        {
            pattern += " ";
        }
        if (add)
        {
            m_patterns.insert(patterns[i]);
        }
        else
        {
            m_patterns.erase(patterns[i]);
        }
    }
    return pattern;
}

uint64_t KeySpaceSubscriber::readData()
{   
    redisReply *reply = nullptr;

    /* Read data from redis. This call is non blocking. This method
     * is called from Select framework when data is available in socket.
     * NOTE: All data should be stored in event buffer. It won't be possible to
     * read them second time. */
    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }

    m_keyspace_event_buffer.emplace_back(std::make_shared<RedisReply>(reply));

    /* Try to read data from redis cacher.
     * If data exists put it to event buffer.
     * NOTE: Keyspace event is not persistent and it won't
     * be possible to read it second time. If it is not stared in
     * the buffer it will be lost. */

    reply = nullptr;
    int status;
    do
    {
        status = redisGetReplyFromReader(m_subscribe->getContext(), reinterpret_cast<void**>(&reply));
        if(reply != nullptr && status == REDIS_OK)
        {
            m_keyspace_event_buffer.emplace_back(std::make_shared<RedisReply>(reply));
        }
    }
    while(reply != nullptr && status == REDIS_OK);

    if (status != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }
    return 0;
}

bool KeySpaceSubscriber::popEventBuffer(RedisMessage& msg)
{
    if (m_keyspace_event_buffer.empty())
    {
        return false;
    }

    auto reply = m_keyspace_event_buffer.front();
    msg = reply->getReply<RedisMessage>();
    m_keyspace_event_buffer.pop_front();
    
    /* if the Key-space notification is empty, try next one. */
    if (msg.type.empty())
    {
        return false;
    }

    if (m_patterns.find(msg.pattern) == m_patterns.end())
    {
        SWSS_LOG_ERROR("keyspace notification recieved for unsubscribed pattern %s", msg.pattern.c_str());
        return false;
    }

    return true;
}

void KeySpaceSubscriber::pops(std::deque<RedisMessage> &dQ)
{   
    RedisMessage ret;
    while (hasData())
    {
       if (popEventBuffer(ret))
       {
           dQ.push_back(ret);
       }
    }
}

}
