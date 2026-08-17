#!/bin/sh

# Effacement des dependances
# if [ $_RECURSE -eq 1 ] && [ -f ${0%/*}/../Description.xml ] ; then
# {
#    python "${0%/*}/Dependencies.py" -d "${0%/*}/../Description.xml" clean
#    if [ $? -eq 1 ] ; then
#       echo "Dependencies.py failed !"
#       exit 1
#    fi
# }
# fi

# Effacement des répertoire build et dist
if [ -d ${0%/*}/../build ] ; then
{
    echo "Deleting ${0%/*}/../build"
    rm -rf ${0%/*}/../build
}
fi
if [ -d ${0%/*}/../dist ] ; then
{
    echo "Deleting ${0%/*}/../dist"
    rm -rf ${0%/*}/../dist
}
fi
