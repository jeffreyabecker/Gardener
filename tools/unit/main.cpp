// The suite's entry point and nothing else: doctest supplies main, the case filter and the report.
//
// This file must not include <Arduino.h>. The API's Common.h declares `int main()` weak for a board
// build, and that declaration collides with the main doctest defines here.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>
