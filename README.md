# CLIPL - C-Like Imperative Programming Language
## Introduction

CLIPL is an imperative, compiled custom programming language with similar syntax and functionality
as C.

It lets you define variables and functions, can perform arithemtic operations on several data types,
currently including int, string and bool and even has built-in linux syscall support.

The compiler is quite buggy and will **probably crash** often, however the provided examples have been tested and are working.
Some features that are available in the lexer, like floats, records, etc. are **not implemented** in later compiler stages,
so they won't work.

I started this project with the intent to learn more about compilers and not to create a language for real-world use.

## Compiler Features

I tried to incorporate several modern compiler features and techniques.
All elements of the compiler are 100% hand-written (except the assembler):

- Lexer for [tokenization](https://en.wikipedia.org/wiki/Lexical_analysis)
- [Parser](https://en.wikipedia.org/wiki/Parsing) to generate the Abstract Syntax Tree
- Threading routines to create the [Control-Flow Graph](https://en.wikipedia.org/wiki/Control-flow_graph)
- Stack-based symbolic interpreter
- Generator of [intermediate pseudo-assembly](https://en.wikipedia.org/wiki/Intermediate_representation)
- [Live-variable analysis](https://en.wikipedia.org/wiki/Live-variable_analysis)
- Graph coloring for [register allocation](https://en.wikipedia.org/wiki/Register_allocation#Graph-coloring_allocation)

The final output is a file with [NASM](https://en.wikipedia.org/wiki/Netwide_Assembler) assembly code, which can be assembled in the compiler or manually assembled.

I mostly learned about compiler design from the book [Modern Compiler Design](https://link.springer.com/book/10.1007/978-1-4614-4699-6) and drew some inspiration for the implementation of the
parser and generator from [here](https://github.com/rui314/8cc/).

## Language Features

This program illustrates most of CLIPL's features:

```c
!import std.clipl

fn writeTo(string filename, string msg) -> int
{
	int success;
	int file_descriptor = openFile(filename, 2);

	if (file_descriptor > 0) {
		success = writeToFile(file_descriptor, msg);
	}

	return success;
}

# 'entry' is used to mark any function as the program entrypoint
entry fn main() -> void 
{
	int num = 1 + 5 * (10 / 2);

	printInt(num);

	printString(", ");

	# string concatenation works most of the time
	string str1 = "Hello ";
	string str2 = "World";
	string msg = str1 + str2 + "!";

	printString(msg);

	bool b = true;

	int counter = 0;

	while (counter < 10) {
		counter += 1;
	}

	int success = writeTo("test.txt", msg);

	# array concatenation works most of the time
	int array[10] = [0, 1, 2, 2, 4, 5] + [6, 7, 8, 9];

	if (array[3] != 3) {
		array[3] = 3;
	}

	int counter1 = 0;
	int counter2 = 0;

	for (int i : array) {
		counter1 += i;
	}

	for (int j : range(0, 10)) {
		counter2 += i;
	}

	printString(", ");

	if (counter1 != counter2) {
		printString("Success!");
	} else {
		printString("Failure!");
	}
}

```

## Command-Line Options
```
Usage: clipl <file> [options]

Options:
-o		Specify the name of the output file
-s		Output assembly
-d[type]	Specify which debug outputs should be generated
-dlex		Print lexer output
-dast		Print abstract syntax tree output
-dcfg		Print control-flow graph output
-dsym		Print symbolic interpreter output
-dlive		Print lva and graph-colorer output
-dps		Print pseudo-assembly output(collides with above option)
-D		Show all debug output (except -dps)
-h		Print this help page
```

## Installation

The compiler can simply be built with running ```make```.
However, running the program without the ```-s``` flag requires ```nasm``` and ```ld``` to be installed.
