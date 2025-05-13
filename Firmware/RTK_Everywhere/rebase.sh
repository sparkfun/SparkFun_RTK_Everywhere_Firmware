#!/bin/bash
#
# rebase.sh
#    Script to rebase branches on top of one another
#    Branches are listed in the file: rebase.dat
########################################################################
#set -e
#set -o verbose
#set -o xtrace
#set -o pipefail

# Files
data_file=rebase.dat

# Rebase the branches on top of one another
while  read  branch_name
do
    if [[ -n "$base_branch" ]]
    then
        # Rebase the branch
        git checkout $branch_name
        git rebase -i $base_branch < /dev/tty
    else
        # Start with everything clean
        clear
        git reset --hard HEAD
    fi

    # Set the new base branch
    base_branch=$branch_name
done < $data_file
