#!/bin/bash
clear

if [ "$#" -lt 3 ]; then
    echo 'Error: need insert at least 3 aregument'
    exit 1
fi

echo 'This script has one goal: never write a full syscall again!'
echo 'Please dont blame if there is a question on the test to write syscall and you cant remember how to do it'
echo 'This script going to update 5 files:' 

echo '-------------------------------------------------------------------------'
echo '1. syscall.h'

lastCallNum=$(grep -oE '[^ ]+$' syscall.h | tail -n 1)
echo 'The last syscall num is '$lastCallNum
echo 'Adding new syscall with syscall num '$((lastCallNum+1))

if [ "$(tail -c 1 syscall.h)" ]; then
    echo '' >> syscall.h
fi
echo '#define SYS_'$2' '$((lastCallNum+1)) >> syscall.h

echo 'syscall.h updating is done'
echo '-------------------------------------------------------------------------'

echo '-------------------------------------------------------------------------'
echo '2. syscall.c'

func='extern int sys_'$2'(void);'
old='extern int sys_chdir(void);'
sed -i "s/$old/${func}\n${old}/" syscall.c
func='[SYS_'$2']\t\tsys_'$2','
old='static int (\*syscalls\[\])(void) = {'
sed -i "s/$old/${old}\n${func}/" syscall.c
 
echo 'syscall.c updating is done'
echo '-------------------------------------------------------------------------'

echo '-------------------------------------------------------------------------'
echo '3. sysproc.c'

echo "" >> sysproc.c
echo "int" >> sysproc.c
echo "sys_$2(void)" >> sysproc.c
echo "{" >> sysproc.c
echo "  return 0;" >> sysproc.c
echo "}" >> sysproc.c

echo 'sysproc.c updating is done'
echo '-------------------------------------------------------------------------'


echo '-------------------------------------------------------------------------'
echo '4. usys.S'

if [ "$(tail -c 1 usys.S)" ]; then
    echo '' >> usys.S
fi

echo 'SYSCALL('$2')' >> usys.S

echo 'usys.S updating is done'
echo '-------------------------------------------------------------------------'

echo '-------------------------------------------------------------------------'
echo '5. user.h'

set -f
func=$1' '$2'('${@:3}');'
set +f
old='int fork(void);'
sed -i "s/${old}/${func}\n${old}/" user.h 


echo 'user.h updating is done'
echo '-------------------------------------------------------------------------'
