
#include <stdio.h>

void test();

void sle_print(int i) { fprintf(stderr, "%d\n", i); }

int main(int argc, char *argv[]) {
  test();

  return 0;
}
