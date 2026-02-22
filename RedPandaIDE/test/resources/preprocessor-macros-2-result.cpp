#define EMPTY
#define SCAN(x)     x
#define EXAMPLE_()  EXAMPLE
#define EXAMPLE(n)  EXAMPLE_ EMPTY()(n-1) (n)
EXAMPLE_()(5-1)(5)
EXAMPLE_()(5-1-1)(5-1)(5)