#!/bin/bash
#
# check_branches.sh
#    Script to check various branches work with #defines compiled out.
#    Branches are listed in the file: check-branches.dat
########################################################################
#set -e
#set -o verbose
#set -o xtrace
set -o pipefail

# Files
data_file=check_branches.dat
log_file=check_branches.log

# Remove the log file
if [ -f "$log_file" ] ; then
    rm "$log_file"
fi

# Get the starting and ending branches
while  read  branch_name
do
    if [[ -n "$start_branch" ]]
    then
        clear
        break;
    else
        start_branch=$branch_name
    fi
    end_branch=$branch_name
done < $data_file

# Display the branches being checked
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Checking branches: $start_branch  -->  $end_branch"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file

# Walk the list of branches, log the output
previous_branch=$start_branch
while  read  branch_name
do
    ./check_named_branch.sh  $branch_name  2>&1  |  tee  -a  $log_file
    exit_status=$?
    if [ $exit_status -ne 0 ]; then
        if [ "$previous_branch" != "$branch_name" ]; then
            # Display the verified commits
            echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
            echo "    Verified branches:  $start_branch  -->  $previous_branch"  2>&1  |  tee  -a  $log_file
            date  2>&1  |  tee  -a  $log_file
            echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
            while  read  branch
            do
                if [ "$branch" == "$branch_name" ]; then
                    break;
                fi
                echo $branch  2>&1  |  tee  -a  $log_file
            done < $data_file
        fi
        echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
        echo "Error: Branch $branch_name failed!"  2>&1  |  tee  -a  $log_file
        echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
        echo "check_branches exiting with status: $exit_status"  2>&1  |  tee  -a  $log_file
        exit $exit_status
    fi
    previous_branch=$branch_name
done < $data_file

# Display the verified branches
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Verified branches"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
while  read  branch
do
    echo $branch  2>&1  |  tee  -a  $log_file
done < $data_file

# Display the success message
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
echo "    Successfully verified all branches  $start_branch  -->  $end_branch"  2>&1  |  tee  -a  $log_file
date  2>&1  |  tee  -a  $log_file
echo "--------------------------------------------------------------------------------"  2>&1  |  tee  -a  $log_file
