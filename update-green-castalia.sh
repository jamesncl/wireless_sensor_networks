#!/bin/bash

# Script to be run from within the vagrant VM to update Green Castalia
# VM installation with development files and build
# Options:
# -b : Build Castalia
# -d : Build debug
# -t : Build a test executable and run 

# Move to the Catalia folder
cd /home/vagrant/Castalia/Castalia
# Make sure the test dir exist (if this is a new installation they may not)
# -p to ignore errors
mkdir -p ./test

# Copy files to the Castalia folders
# use --update so it only copies if newer (avoids unnecessary recompiling of all files)
echo "Copying simulations"
cp --recursive /vagrant/Simulations/* ./Simulations/
echo "Copying src"
cp --recursive --update /vagrant/src/* ./src/
echo "Copying test"
cp --recursive --update /vagrant/test/* ./test
echo "Copying makemake file"
cp /vagrant/makemake ./
echo "Copying bin"
cp --recursive --update /vagrant/bin/* ./bin/

while getopts "brdti" option; do
	case "${option}" in
		b)
			# Build Castalia
			echo "Building Castalia" >&2
			make
			;;
		r)
			# Reuild Castalia
			echo "Rebuilding Castalia" >&2
			make clean
			./makemake
			make
			;;
		d)
			# Debug Castalia
			echo "Building debug Castalia" >&2
			make clean
			./makemake -d
			make
			;;
		t)
			# Test Castalia
			echo "Testing Castalia" >&2
			make clean
			# -d means build debug
			# -t means build test executable instead of simulation executable
			./makemake -d -t
			make
			# Run the test executable
			if [ $? -eq 0 ]; then
				out/gcc-debug/CastaliaTest
			else
				echo "Build failed, not running tests"
				exit 1
			fi
			;;
		i)
			# ! Must have made make file for test (-t) before calling this!
			# Test without full rebuild (incremental)
			make
			# Run the test executable if the build passed
			if [ $? -eq 0 ]; then
				out/gcc-debug/CastaliaTest 
			else
				echo "Build failed, not running tests"
				exit 1
			fi
			;;

		\?)
			echo "Invalid option: -$OPTARG" >&2
			;;
	esac
done


