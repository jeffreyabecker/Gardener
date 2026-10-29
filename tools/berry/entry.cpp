// The main the C runtime looks for, and nothing else in it: the API's Common.h declares the `int
// main()` a board's core calls a sketch's setup() and loop() from, and C++ will not hold that beside
// an entry taking the arguments this tool is driven with. So main.cpp is hostMain, which may include
// the API, and this translation unit includes nothing.

int hostMain(int argc, char** argv);

int main(int argc, char** argv) { return hostMain(argc, argv); }
