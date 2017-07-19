
/* based on get_qualified_type() in 403.gcc of SPEC CPU2006 v1.1 */

int test(int type, int type_quals) {
  int t;
  type = 4;
  type_quals = 3;

  for (t = type; t; t--)
    if (t == type_quals && t) {
      sle_print(t);
      return t;
    }

  sle_print(-1);
  return -1;
}
