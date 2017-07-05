
void sle_print(int);

void foo() {
  int i = 10;
  int a = 0;

  while (--i) {
    a++;
  }

  sle_print(a);
  sle_print(i);
}
