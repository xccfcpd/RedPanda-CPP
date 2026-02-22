#define EMPTY
#define SCAN(x)     x
#define EXAMPLE_()  EXAMPLE
#define EXAMPLE(n)  EXAMPLE_ EMPTY()(n-1) (n)
EXAMPLE(5)
SCAN(EXAMPLE(5))