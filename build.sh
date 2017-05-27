#!/bin/bash

OUTPUT=out/
make clean
make

if [ -d $OUTPUT ]
then
	rm -fr $OUTPUT
fi

mkdir -p $OUTPUT

cp -fr liblre.a $OUTPUT
cp -fr liblre.so $OUTPUT
cp -fr lrexe $OUTPUT

cp -fr global_macro.lr $OUTPUT

cp -fr samples/samples $OUTPUT
cp -fr samples/lrtests $OUTPUT
cp -fr samples/tests.conf $OUTPUT
