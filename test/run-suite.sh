#! /bin/sh

function compress () {
cat $1 | tr "[\n\r]" ["  "] | tr -s "[:space:]" > $1.tmp
}

function check1 () {
node test/suite/$1.js > .obj/testNode.txt
./run.sh test/suite/$1.js
.obj/tmp.exe > .obj/testExe.txt
compress .obj/testNode.txt
compress .obj/testExe.txt
cmp .obj/testNode.txt.tmp .obj/testExe.txt.tmp
if [ $? -eq 0 ]; then
    echo Passed: $1
else
    echo Failed: $1
fi
# rm -rf .obj/testNode.txt.tmp .obj/testExe.txt.tmp
}

#
# run the tests
#

check1 bigint
check1 new_constructors
check1 object_expression
check1 set_property_primitive
check1 with_scoping
