#include <string>
#include <memory>
#include <hiredis/hiredis.h>
#include "logger.h"
#include "dbconnector.h"
#include "keyspacesubscriber.h"

namespace swss {

KeySpaceSubscriber::KeySpaceSubscriber(const std::string &dbName, int pri) : RedisSelect(pri)
{
    m_subscribe = std::make_unique<DBConnector>(dbName, 1000);
    m_subscribe->setClientName("KeySpaceSubscriber");
}

void KeySpaceSubscriber::subscribe(const std::string &pattern)
{
    m_subscribe->subscribe(pattern);
}

void KeySpaceSubscriber::psubscribe(const std::string &pattern)
{
    m_subscribe->psubscribe(pattern);
}

void KeySpaceSubscriber::punsubscribe(const std::string &pattern)
{
    m_subscribe->punsubscribe(pattern);
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

std::shared_ptr<RedisReply> KeySpaceSubscriber::popEventBuffer()
{
    if (m_keyspace_event_buffer.empty())
    {
        return NULL;
    }

    auto reply = m_keyspace_event_buffer.front();
    m_keyspace_event_buffer.pop_front();

    return reply;
}

std::deque<std::string> KeySpaceSubscriber::pops()
{
    std::string reply;
    std::deque<std::string> vkco;
    while (auto event = popEventBuffer())
    {   
       vkco.push_back(event->to_string());
       SWSS_LOG_NOTICE("Event: %s", event->to_string().c_str());
    }
    return vkco;
}

}
