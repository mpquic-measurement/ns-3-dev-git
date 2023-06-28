#!/bin/bash

set -x

echo "Setting up routes..."
# By default, docker containers don't compute UDP / TCP checksums.
# When packets run through ns3 however, the receiving endpoint requires valid checksums.
# This command makes sure that the endpoints set the checksum on outgoing packets.
ethtool -K eth0 tx off
ifconfig eth0 promisc

