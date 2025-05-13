#!/bin/bash
#
# check_commits.sh
#    Script to walk the list of commits and verify that each builds
#    successfully with the various defines commented out
#
# Parameters:
#    $1 - Ending revision (defaults to HEAD)
#    $2 - Starting revision (defaults to release_candidate)
########################################################################
#set -e
#set -o verbose
#set -o xtrace
set -o pipefail

# Set the default values
end_revision=${1:-HEAD}
start_revision=${2:-release_candidate}

# Files
data_file=check_commits.dat
log_file=check_commits.log

# Remove the log file
if [ -f "$log_file" ] ; then
    rm "$log_file"
fi

# Create the data file
git cherry $start_revision $end_revision > $data_file

# Get the starting and ending commits
while read  operation_type  commit_id
do
    if [[ -n "$start_commit" ]]
    then
        clear
        break;
    else
        start_commit=$commit_id
    fi
    end_commit=$commit_id
done < $data_file

# Display the commits being checked
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Checking commits: $start_revision  -->  $end_revision"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file

# Walk the list of commits, separating the type from the ID
previous_commit=$start_commit
while read  operation_type  commit_id
do
    ./check_named_branch.sh  $commit_id  2>&1  |  tee  -a  $log_file
    exit_status=$?
    if [ $exit_status -ne 0 ]; then
        if [ "$previous_commit" != "$commit_id" ]; then
            # Display the verified commits
            echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
            echo "    Verified commits"  2>&1  |  tee  -a  $log_file
            date  2>&1  |  tee  -a  $log_file
            echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
            while read  operation_type  commit
            do
                echo $commit  2>&1  |  tee  -a  $log_file
                if [ "$commit" == "$previous_commit" ]; then
                    break;
                fi
            done < $data_file
        fi
        echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
        echo "Error: Commit $commit_id failed!"  2>&1  |  tee  -a  $log_file
        echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
        echo "check_commits exiting with status: $exit_status"  2>&1  |  tee  -a  $log_file
        exit $exit_status
    fi
    previous_commit=$commit_id
done < $data_file

# Display the verified commits
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Verified commits"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
while read  operation_type  commit_id
do
    echo $commit_id  2>&1  |  tee  -a  $log_file
done < $data_file

# Display the success message
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Successfully verified all commits:  $start_revision  -->  $end_revision"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
