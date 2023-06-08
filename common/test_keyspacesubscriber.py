from swsscommon.swsscommon import KeySpaceSubscriber, Select, SonicDBConfig
import syslog
import signal
import sys, time

class MockHostConfigDaemon:
    def __init__(self):
        pass

    def format(self, table):
        m_notif = f"__keyspace@4__:{table}|*"
        print(f"Subscribing to {m_notif}")
        return f"__keyspace@4__:{table}|*"

    def subscribe_tables(self, key_space):
        key_space.psubscribe(self.format('KDUMP'))
        # Handle FEATURE updates before other tables
        key_space.psubscribe(self.format('FEATURE'))
        # Handle AAA and TACACS related tables
        key_space.psubscribe(self.format('AAA'))
        key_space.psubscribe(self.format('TACPLUS_SERVER'))
        key_space.psubscribe(self.format('TACPLUS'))
        key_space.psubscribe(self.format('LOOPBACK_INTERFACE'))
        # Handle NTP & NTP_SERVER updates
        key_space.psubscribe(self.format('NTP_SERVER'))
        key_space.psubscribe(self.format('NTP'))
        key_space.psubscribe(self.format('AUTO_TECHSUPPORT'))
        key_space.psubscribe(self.format('PORT'))

    def start(self):
        # self.selector = Select()
        key_space = KeySpaceSubscriber("CONFIG_DB")
        self.subscribe_tables(key_space)
        # self.selector.addSelectable(key_space)
        while True:
            time.sleep(1)
            print("Data Read")
            ret = key_space.readData()
            print(f"Data Read: {ret}")
            print("Event")
            print(key_space.pops())

def signal_handler(sig, frame):
    if sig == signal.SIGHUP:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGHUP' is caught and ignoring..")
    elif sig == signal.SIGINT:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGINT' is caught and exiting...")
        sys.exit(128 + sig)
    elif sig == signal.SIGTERM:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGTERM' is caught and exiting...")
        sys.exit(128 + sig)
    else:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: invalid signal - ignoring..")

def main():
    daemon = MockHostConfigDaemon()
    daemon.start()

if __name__ == "__main__":
    main()