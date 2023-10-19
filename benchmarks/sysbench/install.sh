#!/usr/bin/env bash
set -euxo pipefail

# install
sudo apt-get -y install sysbench=1.0.11+ds-1

set +x
INSTALLED_VERSION=$(sysbench --version 2> /dev/null)
EXPECTED_VERSION="sysbench 1.0.11"

if [ "$INSTALLED_VERSION" = "$EXPECTED_VERSION" ]; then
	echo "sysbench installed"
	echo "    Version: $INSTALLED_VERSION"
else
	echo "sysbench installed, but not the expected version."
	echo "Results and/or commands may NOT be reproducible."
	echo "    Expect: $EXPECTED_VERSION"
	echo "    Got: $INSTALLED_VERSION"
fi

