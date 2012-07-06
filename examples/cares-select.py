
import sys
sys.path.insert(0, '../')

import pyuv
import select
import socket


def wait_channel(channel):
    while True:
        read_fds, write_fds = channel.fds
        if not read_fds and not write_fds:
            break
        timeout = channel.timeout()
        print timeout
        rlist, wlist, xlist = select.select(read_fds, write_fds, [], timeout)
        for fd in rlist:
            channel.process_fd(fd, pyuv.cares.ARES_SOCKET_BAD)
        for fd in wlist:
            channel.process_fd(pyuv.cares.ARES_SOCKET_BAD, fd)


if __name__ == '__main__':
    def cb(result, error):
        print result
        print error
    channel = pyuv.cares.Channel()
    channel.gethostbyname('google.com', socket.AF_INET, cb)
    wait_channel(channel)
    print "Done!"

