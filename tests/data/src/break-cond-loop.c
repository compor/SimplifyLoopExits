
void sle_print(int);

void foo() {
  int i = 10;
  int a = 0;

  while (--i) {
    a++;

    if(a == 5)
      break;

    a++;
  }

  sle_print(i);
  sle_print(a);

  return;
}
