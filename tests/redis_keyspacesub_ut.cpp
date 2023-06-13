#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/keyspacesubscriber.cpp"

using namespace std;
using namespace swss;

#define NUMBER_OF_THREADS    (1) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS     (1000)
#define MAX_FIELDS_DIV      (30) // Testing up to 30 fields objects
#define PRINT_SKIP          (10) // Print + for Producer and - for Subscriber for every 100 ops

static const string testTableName = "UT_REDIS_TABLE";
static const string testTableName2 = "UT_REDIS_TABLE2";

static bool endSubscription = false; // TODO: use a conditional Variable

static int totalEvents = 0;

static inline int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

static inline string key(int index, int keyid)
{
    return string("key_") + to_string(index) + ":" + to_string(keyid);
}

static inline string field(int index, int keyid)
{
    return string("field ") + to_string(index) + ":" + to_string(keyid);
}

static inline string value(int index, int keyid)
{
    if (keyid == 0)
    {
        return string(); // empty
    }

    return string("value ") + to_string(index) + ":" + to_string(keyid);
}

static inline int readNumberAtEOL(const string& str)
{
    if (str.empty())
    {
        return 0;
    }

    auto pos = str.find(":");
    if (pos == str.npos)
    {
        return 0;
    }

    istringstream is(str.substr(pos + 1));

    int ret;
    is >> ret;

    EXPECT_TRUE((bool)is);

    return ret;
}

static inline void validateFields(const string& key, const vector<FieldValueTuple>& f)
{
    unsigned int maxNumOfFields = getMaxFields(readNumberAtEOL(key));
    int i = 0;

    EXPECT_EQ(maxNumOfFields, f.size());

    for (auto fv : f)
    {
        EXPECT_EQ(i, readNumberAtEOL(fvField(fv)));
        EXPECT_EQ(i, readNumberAtEOL(fvValue(fv)));
        i++;
    }
}

static inline void clearDB()
{
    DBConnector db("TEST_DB", 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

static void producerWorker(int index, const std::string& table)
{
    DBConnector db("TEST_DB", 0, true);
    Table p(&db, table);

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        int maxNumOfFields = getMaxFields(i);

        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(index, j), value(index, j));
            fields.push_back(t);
        }

        if ((i % 100) == 0)
        {
            cout << "+" << flush;
        }

        p.set(key(index, i), fields);
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        p.del(key(index, i));
    }
}


static inline std::string getkeySpace(DBConnector *db, std::string table)
{
   return "__keyspace@" + to_string(db->getDbId()) + "__:" + table + ":" + "*";
}

static void subscriberWorker(vector<string> tables)
{
    DBConnector db("TEST_DB", 0, true);

    KeySpaceSubscriber c("TEST_DB");
    
    for (auto table : tables)
    {
         c.psubscribe(getkeySpace(&db, table)); /* Subscribe to multiple channels */
    }

    Select cs;
    cs.addSelectable(&c);

    while(true && !endSubscription)
    {
        Selectable *selectcs = nullptr;
        int ret = cs.select(&selectcs, 1000);
        
        if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("%s select error %s", __PRETTY_FUNCTION__, strerror(errno));
            continue;
        }

        if (ret == Select::TIMEOUT)
        {
            SWSS_LOG_DEBUG("%s select timeout, ", __PRETTY_FUNCTION__);
            cout << "Select Timeout " << endl;
            continue;
        }        
        
        auto Q = c.pops();
        for (size_t i = 0; i < Q.size(); i++) {
            std::cout << Q[i] <<  std::endl;
        }

        totalEvents += (int)Q.size();
    }
}

TEST(KeySpaceSubscriber, testkeySpaceSubscription)
{
    std::unique_ptr<thread> listenerThread;

    clearDB();

    cout << "Starting " << 1 << " subscribers on redis" << endl;

    vector<string> tables_listen = {testTableName, testTableName2};

    totalEvents = 0;
    listenerThread = std::make_unique<thread>(subscriberWorker, tables_listen);

    sleep(1);

    producerWorker(0, testTableName);
    producerWorker(1, testTableName2);

    sleep(2);

    endSubscription = true;
    
    listenerThread->join();

    cout << totalEvents << endl;

    ASSERT_TRUE(totalEvents == NUMBER_OF_OPS * (2 + 2)); /* 2 tables, 1 set and 1 del event */
}
