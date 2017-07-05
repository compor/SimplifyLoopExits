
void sle_print(int);

void foo() {
  int i = 10;
  int a = 0;

  while (--i) {
    a++;

    if (a == 3)
      goto loop_exit_a;

    if (a == 5)
      goto loop_exit_b;

    a++;
  }

  sle_print(a);
  sle_print(i);

loop_exit_a:
  sle_print(a);
  sle_print(i);
  a++;

loop_exit_b:
  sle_print(a);
  sle_print(i);
  a += 2;

  return;
}
