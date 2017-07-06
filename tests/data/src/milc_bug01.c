
/* based on gaugefix() in 433.milc */

void sle_print(int);

void test() {
  int dummy = 13;
  int max_gauge_iter = 5;
  int gauge_iter = 0;
  int current_av = 20;
  int old_av = 0;
  int del_av = 0;
  int gauge_fix_tol = 1;

  for (gauge_iter = 0; gauge_iter < max_gauge_iter; gauge_iter++) {
    sle_print(del_av);

    if (gauge_iter != 0) {
      del_av = current_av - old_av;
      if (del_av < gauge_fix_tol)
        break;
    }
    old_av = current_av;

    if ((gauge_iter % 2) == 2) {
      sle_print(dummy);
    }
  }

  if (current_av == 0)
    sle_print(del_av);
}
