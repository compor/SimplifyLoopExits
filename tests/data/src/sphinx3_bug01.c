
void test() {
  char *line = "#hello";
  int i = 0;
  int res = -1;

  while (line[i++]) {
    if (line[0] != '#') {
      res = 0;
      break;
    }
  }

  sle_print(res);

  return;
}
