#!/bin/bash
gcc -o convert.so convert.c $(yed --print-cflags) $(yed --print-ldflags)
