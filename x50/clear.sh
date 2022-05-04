#!/bin/bash
function pathIsNull() {
    if [ "$(ls -A $1)" = "" ]; then
        # echo "$1 is empty"
        return 0
    else
        # echo "$1 is not empty"
        return 1
    fi
}
function clearEnd() {
    for file in $1/*; do
        if test -f $file; then
            echo $file is file
            sed -i "s/\r//" $file
        else
            echo $file is path
            if [[ $file != '.' && $file != '..' ]]; then
                `pathIsNull $file`
                if [ $? == 1 ]; then
                    clearEnd $file
                else
                    echo "$file is empty"
                fi
            fi
        fi
    done
}

clearEnd $1
#sed -i "s/\r//"
