-- FICO Language specification --

# Global scope:
  - function definitions
  - record definitions

# Local scope:
  - variable declarations
  - variable definitions
  - if, else statements
  - while, for statements
  - return statement
  - function calls

# Primitive types:
  - void
  - int
  - float
  - string
  - n-D arrays of above types

  Variable declaration structure:
    TYPE NAME ';'

  -- Example variable declarations
    int a;
    float b1;
    string _c;


  Variable defintion structure:
    TYPE NAME '=' VALUE ';'
    NAME '=' VALUE ';'
    TYPE NAME '=' IDENTIFIER ';'
    NAME '=' IDENTIFIER ';'

  -- Example variable definitions
    int a = 25;
    int b = 0x7a;
    c = 0b1101001;
    d = number;

    float e1 = 3.21;
    float f2 = .984;

    string _g = "Hello World!";
    string _h = "Hello \"World\"!";

    int i[10] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    float j[] = [2.3, 53.3, .215];
    string k[] = [getString1(), getString2(), getString3()];
    int l[5][2] = [ [1, 2], [3, 4], [5, 6], [7, 8], [9, 0] ];

# Records:
  Record definition structure:
    'record' NAME '{' [ record_field ';' ] '}' ';'
    recordfield -> Variable_Definition, Variable_Declaration

  -- Example record definitions:
    record Person {
	int a;
	float b;
	string a = "Hello World";
    };

# Functions:
  Function definition structure:
    'fn' NAME '(' function_args ')' '->' TYPE '{' function_body '}'

  -- Example function definitions:
    fn main(int argc, char argv[][]) -> int
    {
    	int a = 1;
    }

