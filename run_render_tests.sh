#!/bin/bash

# This script builds and runs tests inside docker
litehtml_path=$1
threads=$2

if [ -z "$litehtml_path" ]; then
	echo "Usage: $0 <litehtml_path> [threads]"
	exit 1
fi

if [ -z "threads" ]; then
	threads=4
fi

if [[ "$IN_DOCKER" = "TRUE" ]]; then
	# We are inside container
	# Prepare build directory
	mkdir -p /tmp/build
	cd /tmp/build
	# Run CMake for testing
	cmake -DLITEHTML_PATH=/litehtml /litehtml-tests
	# Build litehtml
	make -j$threads
	# Run tests
	ctest -j$threads
else
	# We are on the host.
	# Check if the docker image exists
	image=$(docker images -q litehtml-build:latest)
	if [[ -z "$image" ]]; then
		# Build docker image if it doesn't exist
		echo "Building litehtml-build:latest image"
		docker build -t litehtml-build:latest .
	echo
		echo "The image litehtml-build:latest already exists"
	fi
	# Run this script in the docker container
	docker run --rm -it -v "$(pwd):/litehtml-tests" -v "$(realpath $litehtml_path):/litehtml" -e IN_DOCKER=TRUE -u "$(id -u $USER):$(id -g $USER)" litehtml-build:latest /litehtml-tests/run_render_tests.sh $*
fi
