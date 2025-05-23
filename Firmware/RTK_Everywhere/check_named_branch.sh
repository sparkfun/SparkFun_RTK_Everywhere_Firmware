#!/bin/bash
#
# check_named_branch.sh
#    Script to check various branches work with #defines compiled out.
#
# Parameters:
#    $1 - Branch name (defaults to release-candidate)
########################################################################
#set  -e
#set -o verbose
#set -o xtrace

# Set the default values
branch_name=${1:-release_candidate}

# Get the branch
git  checkout  --quiet  $branch_name

# Display the branch
echo "--------------------------------------------------------------------------------"
git  log  --max-count=1  --format=oneline  HEAD
date
echo "--------------------------------------------------------------------------------"

# Verify that the branch builds successfully with defines commented out
./check.sh
exit_status=$?
#echo "check_named_branch exiting with status: $exit_status"
exit $exit_status
