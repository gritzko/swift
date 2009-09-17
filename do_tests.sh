#!/bin/bash

for tst in `ls tests/*test`; do
    $tst
done
