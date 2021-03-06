#include "Omega_h_align.hpp"
#include "Omega_h_array_ops.hpp"
#include "Omega_h_assoc.hpp"
#include "Omega_h_bbox.hpp"
#include "Omega_h_build.hpp"
#include "Omega_h_compare.hpp"
#include "Omega_h_confined.hpp"
#include "Omega_h_eigen.hpp"
#include "Omega_h_hilbert.hpp"
#include "Omega_h_inertia.hpp"
#include "Omega_h_lie.hpp"
#include "Omega_h_linpart.hpp"
#include "Omega_h_loop.hpp"
#include "Omega_h_map.hpp"
#include "Omega_h_most_normal.hpp"
#include "Omega_h_quality.hpp"
#include "Omega_h_recover.hpp"
#include "Omega_h_refine_qualities.hpp"
#include "Omega_h_scan.hpp"
#include "Omega_h_shape.hpp"
#include "Omega_h_sort.hpp"
#include "Omega_h_swap2d.hpp"
#include "Omega_h_swap3d_choice.hpp"
#include "Omega_h_swap3d_loop.hpp"
#include "Omega_h_vtk.hpp"
#include "Omega_h_xml.hpp"

#ifdef OMEGA_H_USE_TEUCHOSPARSER
#include "Omega_h_expr.hpp"
#endif

#include <iostream>
#include <sstream>

using namespace Omega_h;

template <Int m, Int n>
static void test_qr_decomp(Matrix<m, n> a) {
  auto qr = factorize_qr_householder(m, n, a);
  auto r = qr.r;
  auto q = identity_matrix<m, n>();
  for (Int j = 0; j < n; ++j) implicit_q_x(m, n, q[j], qr.v);
  OMEGA_H_CHECK(are_close(a, q * r));
  OMEGA_H_CHECK(are_close(transpose(q) * q, identity_matrix<n, n>()));
}

static void test_qr_decomps() {
  test_qr_decomp(identity_matrix<3, 3>());
  test_qr_decomp(Matrix<3, 3>({EPSILON, 0, 0, 0, EPSILON, 0, 0, 0, EPSILON}));
  test_qr_decomp(Matrix<3, 3>({12, -51, 4, 6, 167, -68, -4, 24, -41}));
}

static void test_form_ortho_basis() {
  auto n = normalize(vector_3(1, 1, 1));
  auto f = form_ortho_basis(n);
  OMEGA_H_CHECK(are_close(f[0], n));
  OMEGA_H_CHECK(are_close(transpose(f) * f, identity_matrix<3, 3>()));
}

static void test_least_squares() {
  Matrix<4, 2> m({1, 1, 1, 2, 1, 3, 1, 4});
  Vector<4> b({6, 5, 7, 10});
  auto x = solve_using_qr(m, b);
  OMEGA_H_CHECK(are_close(x, vector_2(3.5, 1.4)));
}

static void test_int128() {
  Int128 a(INT64_MAX);
  auto b = a + a;
  b = b + b;
  b = b + b;
  b = b >> 3;
  OMEGA_H_CHECK(b == a);
}

static void test_repro_sum() {
  Reals a({std::exp2(int(20)), std::exp2(int(-20))});
  Real sum = repro_sum(a);
  OMEGA_H_CHECK(sum == std::exp2(20) + std::exp2(int(-20)));
}

static void test_power() {
  auto x = 3.14159;
  OMEGA_H_CHECK(x == power(x, 1, 1));
  OMEGA_H_CHECK(x == power(x, 2, 2));
  OMEGA_H_CHECK(x == power(x, 3, 3));
  OMEGA_H_CHECK(are_close(x * x, power(x, 2, 1)));
  OMEGA_H_CHECK(are_close(x * x * x, power(x, 3, 1)));
  OMEGA_H_CHECK(are_close(std::sqrt(x), power(x, 1, 2)));
  OMEGA_H_CHECK(are_close(std::cbrt(x), power(x, 1, 3)));
  OMEGA_H_CHECK(are_close(std::sqrt(x * x * x), power(x, 3, 2)));
  OMEGA_H_CHECK(are_close(std::cbrt(x * x), power(x, 2, 3)));
}

static void test_cubic(Few<Real, 3> coeffs, Int nroots_wanted,
    Few<Real, 3> roots_wanted, Few<Int, 3> mults_wanted) {
  auto roots = find_polynomial_roots(coeffs);
  OMEGA_H_CHECK(roots.n == nroots_wanted);
  for (Int i = 0; i < roots.n; ++i) {
    OMEGA_H_CHECK(roots.mults[i] == mults_wanted[i]);
    OMEGA_H_CHECK(are_close(roots.values[i], roots_wanted[i]));
  }
}

static void test_cubic() {
  test_cubic({0, 0, 0}, 1, Few<Real, 3>({0}), Few<Int, 3>({3}));
  test_cubic({1., -3. / 2., -3. / 2.}, 3, Few<Real, 3>({2, -1, .5}),
      Few<Int, 3>({1, 1, 1}));
  test_cubic({2., -3., 0}, 2, Few<Real, 3>({-2, 1}), Few<Int, 3>({1, 2}));
  test_cubic({-8, -6, 3}, 3, Few<Real, 3>({2, -4, -1}), Few<Int, 3>({1, 1, 1}));
}

template <Int dim>
static void test_eigen(
    Matrix<dim, dim> m, Matrix<dim, dim> q_expect, Vector<dim> l_expect) {
  auto ed = decompose_eigen(m);
  auto q = ed.q;
  auto l = ed.l;
  OMEGA_H_CHECK(are_close(q, q_expect));
  OMEGA_H_CHECK(are_close(l, l_expect));
}

template <Int dim>
static void test_eigen(Matrix<dim, dim> m, Vector<dim> l_expect) {
  auto ed = decompose_eigen(m);
  auto q = ed.q;
  auto l = ed.l;
  OMEGA_H_CHECK(are_close(l, l_expect, 1e-8, 1e-8));
  OMEGA_H_CHECK(are_close(m, compose_eigen(q, l)));
}

static void test_eigen_cubic_ortho(Matrix<3, 3> m, Vector<3> l_expect) {
  auto ed = decompose_eigen(m);
  auto q = ed.q;
  auto l = ed.l;
  OMEGA_H_CHECK(
      are_close(transpose(q) * q, identity_matrix<3, 3>(), 1e-8, 1e-8));
  OMEGA_H_CHECK(are_close(l, l_expect, 1e-8, 1e-8));
  OMEGA_H_CHECK(are_close(m, compose_ortho(q, l), 1e-8, 1e-8));
}

static void test_eigen_metric(Vector<3> h) {
  auto q =
      rotate(PI / 4., vector_3(0, 0, 1)) * rotate(PI / 4., vector_3(0, 1, 0));
  OMEGA_H_CHECK(are_close(transpose(q) * q, identity_matrix<3, 3>()));
  auto l = metric_eigenvalues_from_lengths(h);
  auto a = compose_ortho(q, l);
  test_eigen_cubic_ortho(a, l);
}

static void test_eigen_quadratic() {
  test_eigen(identity_matrix<2, 2>(), identity_matrix<2, 2>(), vector_2(1, 1));
  test_eigen(zero_matrix<2, 2>(), identity_matrix<2, 2>(), vector_2(0, 0));
  test_eigen(matrix_2x2(8.67958, -14.0234, -1.04985, 2.25873),
      matrix_2x2(9.9192948778227130e-01, 8.6289280817702185e-01,
          -1.2679073810022995e-01, 5.0538698202107812e-01),
      vector_2(1.0472083659357935e+01, 4.6622634064206342e-01));
}

static void test_eigen_cubic() {
  test_eigen(
      identity_matrix<3, 3>(), identity_matrix<3, 3>(), vector_3(1, 1, 1));
  test_eigen(zero_matrix<3, 3>(), identity_matrix<3, 3>(), vector_3(0, 0, 0));
  test_eigen(matrix_3x3(-1, 3, -1, -3, 5, -1, -3, 3, 1), vector_3(1, 2, 2));
  /* the lengths have to be ordered so that
     if two of them are the same they should
     appear at the end */
  test_eigen_metric(vector_3(1e+3, 1, 1));
  test_eigen_metric(vector_3(1, 1e+3, 1e+3));
  test_eigen_metric(vector_3(1e-3, 1, 1));
  test_eigen_metric(vector_3(1, 1e-3, 1e-3));
  test_eigen_metric(vector_3(1e-6, 1e-3, 1e-3));
}

template <Int dim>
static void test_eigen_jacobi(
    Matrix<dim, dim> a, Matrix<dim, dim> expect_q, Vector<dim> expect_l) {
  auto ed = decompose_eigen_jacobi(a);
  ed = sort_by_magnitude(ed);
  OMEGA_H_CHECK(are_close(ed.q, expect_q));
  OMEGA_H_CHECK(are_close(ed.l, expect_l));
}

static void test_eigen_jacobi_sign_bug() {
  auto sign_bug_input = matrix_3x3(0.99999999998511147, 5.3809065327405379e-11,
      9.7934015130085018e-10, 5.3809065327405379e-11, 0.99999999995912181,
      -1.676999986436999e-09, 9.7934015130085018e-10, -1.676999986436999e-09,
      0.99999995816580101);
  decompose_eigen_jacobi(sign_bug_input);
}

static void test_eigen_jacobi() {
  test_eigen_jacobi(
      identity_matrix<2, 2>(), identity_matrix<2, 2>(), vector_2(1, 1));
  test_eigen_jacobi(
      identity_matrix<3, 3>(), identity_matrix<3, 3>(), vector_3(1, 1, 1));
  test_eigen_jacobi(matrix_2x2(2, 1, 1, 2),
      matrix_2x2(1, 1, 1, -1) / std::sqrt(2), vector_2(3, 1));
  test_eigen_jacobi(matrix_3x3(2, 0, 0, 0, 3, 4, 0, 4, 9),
      Matrix<3, 3>({normalize(vector_3(0, 1, 2)), normalize(vector_3(1, 0, 0)),
          normalize(vector_3(0, 2, -1))}),
      vector_3(11, 2, 1));
  test_eigen_jacobi_sign_bug();
}

static void test_intersect_ortho_metrics(
    Vector<3> h1, Vector<3> h2, Vector<3> hi_expect) {
  auto q =
      rotate(PI / 4., vector_3(0, 0, 1)) * rotate(PI / 4., vector_3(0, 1, 0));
  auto m1 = compose_metric(q, h1);
  auto m2 = compose_metric(q, h2);
  auto mi = intersect_metrics(m1, m2);
  /* if we decompose it, the eigenvectors may
     get re-ordered. */
  for (Int i = 0; i < 3; ++i) {
    OMEGA_H_CHECK(
        are_close(metric_desired_length(mi, q[i]), hi_expect[i], 1e-3));
  }
}

static void test_intersect_subset_metrics() {
  auto h1 = vector_2(1, 2);
  auto r1 = identity_matrix<2, 2>();
  auto h2 = vector_2(2, 3);
  auto r2 = rotate(PI / 4);
  auto m1 = compose_metric(r1, h1);
  auto m2 = compose_metric(r2, h2);
  OMEGA_H_CHECK(are_close(intersect_metrics(m2, m1), m1));
  OMEGA_H_CHECK(are_close(intersect_metrics(m1, m2), m1));
}

static void test_intersect_with_null() {
  auto q =
      rotate(PI / 4., vector_3(0, 0, 1)) * rotate(PI / 4., vector_3(0, 1, 0));
  auto m1 = compose_metric(q, vector_3(1, 1, 1e-3));
  auto m2 = zero_matrix<3, 3>();
  OMEGA_H_CHECK(are_close(intersect_metrics(m1, m2), m1));
  OMEGA_H_CHECK(are_close(intersect_metrics(m2, m1), m1));
}

static void test_intersect_degen_metrics() {
  if ((0)) {
    test_intersect_with_null();
    // 2.a
    OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 0)),
                                diagonal(vector_3(0, 0, 1))),
        diagonal(vector_3(1, 0, 1))));
    // 2.b
    OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 0)),
                                diagonal(vector_3(2, 0, 0))),
        diagonal(vector_3(2, 0, 0))));
    // 3.a
    OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 0)),
                                diagonal(vector_3(2, 1, 0))),
        diagonal(vector_3(2, 1, 0))));
  }
  // 3.b
  OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 0)),
                              diagonal(vector_3(0, 1, 2))),
      diagonal(vector_3(1, 1, 2))));
  // 4.a
  OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 2)),
                              diagonal(vector_3(2, 0, 1))),
      diagonal(vector_3(2, 0, 2))));
  // 4.b
  OMEGA_H_CHECK(are_close(intersect_metrics(diagonal(vector_3(1, 0, 2)),
                              diagonal(vector_3(2, 1, 0))),
      diagonal(vector_3(2, 1, 2))));
}

static void test_intersect_metrics() {
  test_intersect_ortho_metrics(
      vector_3(0.5, 1, 1), vector_3(1, 0.5, 1), vector_3(0.5, 0.5, 1));
  test_intersect_ortho_metrics(
      vector_3(1e-2, 1, 1), vector_3(1, 1, 1e-2), vector_3(1e-2, 1, 1e-2));
  test_intersect_ortho_metrics(vector_3(1e-2, 1e-2, 1), vector_3(1, 1, 1e-2),
      vector_3(1e-2, 1e-2, 1e-2));
  test_intersect_ortho_metrics(vector_3(1e-5, 1e-3, 1e-3),
      vector_3(1e-3, 1e-3, 1e-5), vector_3(1e-5, 1e-3, 1e-5));
  test_intersect_subset_metrics();
  test_intersect_degen_metrics();
}

static void test_sort() {
  {
    LOs a({0, 1});
    LOs perm = sort_by_keys(a);
    OMEGA_H_CHECK(perm == LOs({0, 1}));
  }
  {
    LOs a({0, 2, 0, 1});
    LOs perm = sort_by_keys(a, 2);
    OMEGA_H_CHECK(perm == LOs({1, 0}));
  }
  {
    LOs a({0, 2, 1, 1});
    LOs perm = sort_by_keys(a, 2);
    OMEGA_H_CHECK(perm == LOs({0, 1}));
  }
  {
    LOs a({1, 2, 3, 1, 2, 2, 3, 0, 0});
    LOs perm = sort_by_keys(a, 3);
    OMEGA_H_CHECK(perm == LOs({1, 0, 2}));
  }
}

static void test_scan() {
  {
    LOs scanned = offset_scan(LOs(3, 1));
    OMEGA_H_CHECK(scanned == Read<LO>(4, 0, 1));
  }
  {
    LOs scanned = offset_scan(Read<I8>(3, 1));
    OMEGA_H_CHECK(scanned == Read<LO>(4, 0, 1));
  }
}

static void test_fan_and_funnel() {
  OMEGA_H_CHECK(invert_funnel(LOs({0, 0, 1, 1, 2, 2}), 3) == LOs({0, 2, 4, 6}));
  OMEGA_H_CHECK(invert_fan(LOs({0, 2, 4, 6})) == LOs({0, 0, 1, 1, 2, 2}));
  OMEGA_H_CHECK(invert_funnel(LOs({0, 0, 0, 2, 2, 2}), 3) == LOs({0, 3, 3, 6}));
  OMEGA_H_CHECK(invert_fan(LOs({0, 3, 3, 6})) == LOs({0, 0, 0, 2, 2, 2}));
  OMEGA_H_CHECK(invert_funnel(LOs({0, 0, 0, 0, 0, 0}), 3) == LOs({0, 6, 6, 6}));
  OMEGA_H_CHECK(invert_fan(LOs({0, 6, 6, 6})) == LOs({0, 0, 0, 0, 0, 0}));
  OMEGA_H_CHECK(invert_funnel(LOs({2, 2, 2, 2, 2, 2}), 3) == LOs({0, 0, 0, 6}));
  OMEGA_H_CHECK(invert_fan(LOs({0, 0, 0, 6})) == LOs({2, 2, 2, 2, 2, 2}));
}

static void test_permute() {
  Reals data({0.1, 0.2, 0.3, 0.4});
  LOs perm({3, 2, 1, 0});
  Reals permuted = unmap(perm, data, 1);
  OMEGA_H_CHECK(permuted == Reals({0.4, 0.3, 0.2, 0.1}));
  Reals back = permute(permuted, perm, 1);
  OMEGA_H_CHECK(back == data);
}

static void test_invert_map() {
  {
    LOs hl2l({}, "hl2l");
    auto l2hl = invert_map_by_atomics(hl2l, 4);
    OMEGA_H_CHECK(l2hl.a2ab == LOs(5, 0));
    OMEGA_H_CHECK(l2hl.ab2b == LOs({}));
  }
  {
    LOs hl2l({0, 1, 2, 3}, "hl2l");
    auto l2hl = invert_map_by_atomics(hl2l, 4);
    OMEGA_H_CHECK(l2hl.a2ab == LOs(5, 0, 1));
    OMEGA_H_CHECK(l2hl.ab2b == LOs(4, 0, 1));
  }
  {
    LOs hl2l({}, "hl2l");
    auto l2hl = invert_map_by_sorting(hl2l, 4);
    OMEGA_H_CHECK(l2hl.a2ab == LOs(5, 0));
    OMEGA_H_CHECK(l2hl.ab2b == LOs({}));
  }
  {
    LOs hl2l({0, 1, 2, 3}, "hl2l");
    auto l2hl = invert_map_by_sorting(hl2l, 4);
    OMEGA_H_CHECK(l2hl.a2ab == LOs(5, 0, 1));
    OMEGA_H_CHECK(l2hl.ab2b == LOs(4, 0, 1));
  }
  {
    LOs hl2l({1, 0, 1, 0}, "hl2l");
    auto l2hl = invert_map_by_sorting(hl2l, 2);
    OMEGA_H_CHECK(l2hl.a2ab == LOs({0, 2, 4}));
    OMEGA_H_CHECK(l2hl.ab2b == LOs({1, 3, 0, 2}));
  }
}

static void test_invert_adj() {
  Adj tris2verts(LOs({0, 1, 2, 2, 3, 0}));
  Adj verts2tris = invert_adj(tris2verts, 3, 4);
  OMEGA_H_CHECK(verts2tris.a2ab == offset_scan(LOs({2, 1, 2, 1})));
  OMEGA_H_CHECK(verts2tris.ab2b == LOs({0, 1, 0, 0, 1, 1}));
  OMEGA_H_CHECK(
      verts2tris.codes ==
      Read<I8>({make_code(0, 0, 0), make_code(0, 0, 2), make_code(0, 0, 1),
          make_code(0, 0, 2), make_code(0, 0, 0), make_code(0, 0, 1)}));
}

static OMEGA_H_DEVICE bool same_adj(Int a[], Int b[]) {
  for (Int i = 0; i < 3; ++i)
    if (a[i] != b[i]) return false;
  return true;
}

static void test_tri_align() {
  auto f = OMEGA_H_LAMBDA(LO) {
    Int ident[3] = {0, 1, 2};
    Int out[3];
    Int out2[3];
    /* check that flipping and rotating do what we want */
    {
      align_adj<3>(make_code(true, 0, 0), ident, 0, out, 0);
      Int expect[3] = {0, 2, 1};
      OMEGA_H_CHECK(same_adj(out, expect));
    }
    {
      align_adj<3>(make_code(false, 1, 0), ident, 0, out, 0);
      Int expect[3] = {2, 0, 1};
      OMEGA_H_CHECK(same_adj(out, expect));
    }
    {
      align_adj<3>(make_code(false, 2, 0), ident, 0, out, 0);
      Int expect[3] = {1, 2, 0};
      OMEGA_H_CHECK(same_adj(out, expect));
    }
    /* check that compound_alignments does its job */
    for (I8 rot1 = 0; rot1 < 3; ++rot1)
      for (I8 flip1 = 0; flip1 < 2; ++flip1)
        for (I8 rot2 = 0; rot2 < 3; ++rot2)
          for (I8 flip2 = 0; flip2 < 2; ++flip2) {
            I8 code1 = make_code(flip1, rot1, 0);
            I8 code2 = make_code(flip2, rot2, 0);
            align_adj<3>(code1, ident, 0, out, 0);
            align_adj<3>(code2, out, 0, out2, 0);
            Int out3[3];
            I8 code3 = compound_alignments<3>(code1, code2);
            align_adj<3>(code3, ident, 0, out3, 0);
            OMEGA_H_CHECK(same_adj(out2, out3));
          }
  };
  parallel_for(1, f);
}

static void test_form_uses() {
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2}), OMEGA_H_SIMPLEX, 2, 1) ==
                LOs({0, 1, 1, 2, 2, 0}));
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2, 3}), OMEGA_H_SIMPLEX, 3, 1) ==
                LOs({0, 1, 1, 2, 2, 0, 0, 3, 1, 3, 2, 3}));
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2, 3}), OMEGA_H_SIMPLEX, 3, 2) ==
                LOs({0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3}));
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2, 3}), OMEGA_H_HYPERCUBE, 2, 1) ==
                LOs({0, 1, 1, 2, 2, 3, 3, 0}));
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2, 3, 4, 5, 6, 7, 8}), OMEGA_H_HYPERCUBE,
                    3, 1) == LOs({0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3,
                                 7, 4, 5, 5, 6, 6, 7, 7, 4}));
  OMEGA_H_CHECK(form_uses(LOs({0, 1, 2, 3, 4, 5, 6, 7, 8}), OMEGA_H_HYPERCUBE,
                    3, 2) == LOs({0, 3, 2, 1, 0, 1, 5, 4, 1, 2, 6, 5, 2, 3, 7,
                                 6, 3, 0, 4, 7, 4, 5, 6, 7}));
}

static void test_reflect_down() {
  Adj a;
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_SIMPLEX, 0, 2, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_SIMPLEX, 0, 3, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_SIMPLEX, 0, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(
      LOs({0, 1, 2}), LOs({0, 1, 1, 2, 2, 0}), OMEGA_H_SIMPLEX, 3, 2, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 2}));
  OMEGA_H_CHECK(a.codes == Read<I8>({0, 0, 0}));
  a = reflect_down(LOs({0, 1, 2, 3}), LOs({0, 1, 1, 2, 2, 0, 0, 3, 1, 3, 2, 3}),
      OMEGA_H_SIMPLEX, 4, 3, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 2, 3, 4, 5}));
  OMEGA_H_CHECK(a.codes == Read<I8>({0, 0, 0, 0, 0, 0}));
  a = reflect_down(LOs({0, 1, 2, 3}), LOs({0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3}),
      OMEGA_H_SIMPLEX, 4, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 2, 3}));
  OMEGA_H_CHECK(a.codes == Read<I8>({0, 0, 0, 0}));
  a = reflect_down(LOs({0, 1, 2, 3}), LOs({0, 1, 2, 0, 3, 1, 1, 3, 2, 2, 3, 0}),
      OMEGA_H_SIMPLEX, 4, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 2, 3}));
  OMEGA_H_CHECK(a.codes == Read<I8>(4, make_code(true, 0, 0)));
  a = reflect_down(LOs({0, 1, 2, 2, 3, 0}), LOs({0, 1, 1, 2, 2, 3, 3, 0, 0, 2}),
      OMEGA_H_SIMPLEX, 4, 2, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 4, 2, 3, 4}));
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_HYPERCUBE, 0, 2, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_HYPERCUBE, 0, 3, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(LOs({}), LOs({}), OMEGA_H_HYPERCUBE, 0, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs({}));
  OMEGA_H_CHECK(a.codes == Read<I8>({}));
  a = reflect_down(LOs({0, 1, 2, 3}), LOs({0, 1, 1, 2, 2, 3, 3, 0}),
      OMEGA_H_HYPERCUBE, 4, 2, 1);
  OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 2, 3}));
  OMEGA_H_CHECK(a.codes == Read<I8>({0, 0, 0, 0}));
  auto hex_verts = LOs(8, 0, 1);
  a = reflect_down(hex_verts,
      LOs({0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7, 4, 5, 5, 6, 6, 7, 7,
          4}),
      OMEGA_H_HYPERCUBE, 8, 3, 1);
  OMEGA_H_CHECK(a.ab2b == LOs(12, 0, 1));
  OMEGA_H_CHECK(a.codes == Read<I8>(12, 0));
  a = reflect_down(hex_verts,
      LOs({0, 3, 2, 1, 0, 1, 5, 4, 1, 2, 6, 5, 2, 3, 7, 6, 3, 0, 4, 7, 4, 5, 6,
          7}),
      OMEGA_H_HYPERCUBE, 8, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs(6, 0, 1));
  OMEGA_H_CHECK(a.codes == Read<I8>(6, 0));
  a = reflect_down(hex_verts,
      LOs({1, 2, 3, 0, 4, 5, 1, 0, 5, 6, 2, 1, 6, 7, 3, 2, 7, 4, 0, 3, 7, 6, 5,
          4}),
      OMEGA_H_HYPERCUBE, 8, 3, 2);
  OMEGA_H_CHECK(a.ab2b == LOs(6, 0, 1));
  OMEGA_H_CHECK(a.codes == Read<I8>(6, make_code(true, 1, 0)));
  // a = reflect_down(
  //    LOs({0, 1, 2, 2, 3, 0}), LOs({0, 1, 1, 2, 2, 3, 3, 0, 0, 2}),
  //    OMEGA_H_HYPERCUBE, 4, 2, 1);
  // OMEGA_H_CHECK(a.ab2b == LOs({0, 1, 4, 2, 3, 4}));
}

static void test_find_unique() {
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_SIMPLEX, 2, 1) == LOs({}));
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_SIMPLEX, 3, 1) == LOs({}));
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_SIMPLEX, 3, 2) == LOs({}));
  OMEGA_H_CHECK(find_unique(LOs({0, 1, 2, 2, 3, 0}), OMEGA_H_SIMPLEX, 2, 1) ==
                LOs({0, 1, 0, 2, 3, 0, 1, 2, 2, 3}));
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_HYPERCUBE, 2, 1) == LOs({}));
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_HYPERCUBE, 3, 1) == LOs({}));
  OMEGA_H_CHECK(find_unique(LOs({}), OMEGA_H_HYPERCUBE, 3, 2) == LOs({}));
  auto a = find_unique(LOs({0, 1, 2, 3}), OMEGA_H_HYPERCUBE, 2, 1);
  OMEGA_H_CHECK(find_unique(LOs({0, 1, 2, 3}), OMEGA_H_HYPERCUBE, 2, 1) ==
                LOs({0, 1, 3, 0, 1, 2, 2, 3}));
}

static void test_hilbert() {
  /* this is the original test from Skilling's paper */
  hilbert::coord_t X[3] = {5, 10, 20};  // any position in 32x32x32 cube
  hilbert::AxestoTranspose(X, 5,
      3);  // Hilbert transpose for 5 bits and 3 dimensions
  std::stringstream stream;
  stream << "Hilbert integer = " << (X[0] >> 4 & 1) << (X[1] >> 4 & 1)
         << (X[2] >> 4 & 1) << (X[0] >> 3 & 1) << (X[1] >> 3 & 1)
         << (X[2] >> 3 & 1) << (X[0] >> 2 & 1) << (X[1] >> 2 & 1)
         << (X[2] >> 2 & 1) << (X[0] >> 1 & 1) << (X[1] >> 1 & 1)
         << (X[2] >> 1 & 1) << (X[0] >> 0 & 1) << (X[1] >> 0 & 1)
         << (X[2] >> 0 & 1) << " = 7865 check";
  std::string expected = "Hilbert integer = 001111010111001 = 7865 check";
  OMEGA_H_CHECK(stream.str() == expected);
  hilbert::coord_t Y[3];
  hilbert::untranspose(X, Y, 5, 3);
  std::stringstream stream2;
  stream2 << "Hilbert integer = " << (Y[0] >> 4 & 1) << (Y[0] >> 3 & 1)
          << (Y[0] >> 2 & 1) << (Y[0] >> 1 & 1) << (Y[0] >> 0 & 1)
          << (Y[1] >> 4 & 1) << (Y[1] >> 3 & 1) << (Y[1] >> 2 & 1)
          << (Y[1] >> 1 & 1) << (Y[1] >> 0 & 1) << (Y[2] >> 4 & 1)
          << (Y[2] >> 3 & 1) << (Y[2] >> 2 & 1) << (Y[2] >> 1 & 1)
          << (Y[2] >> 0 & 1) << " = 7865 check";
  OMEGA_H_CHECK(stream2.str() == expected);
}

static void test_bbox() {
  OMEGA_H_CHECK(are_close(BBox<2>(vector_2(-3, -3), vector_2(3, 3)),
      find_bounding_box<2>(Reals({0, -3, 3, 0, 0, 3, -3, 0}))));
  OMEGA_H_CHECK(are_close(BBox<3>(vector_3(-3, -3, -3), vector_3(3, 3, 3)),
      find_bounding_box<3>(
          Reals({0, -3, 0, 3, 0, 0, 0, 3, 0, -3, 0, 0, 0, 0, -3, 0, 0, 3}))));
}

static void test_build(Library* lib) {
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2}), 3);
    OMEGA_H_CHECK(mesh.ask_down(2, 0).ab2b == LOs({0, 1, 2}));
    OMEGA_H_CHECK(mesh.ask_down(2, 1).ab2b == LOs({0, 2, 1}));
    OMEGA_H_CHECK(mesh.ask_down(1, 0).ab2b == LOs({0, 1, 2, 0, 1, 2}));
  }
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    OMEGA_H_CHECK(mesh.ask_down(3, 0).ab2b == LOs({0, 1, 2, 3}));
  }
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_HYPERCUBE, 2, LOs({0, 1, 2, 3}), 4);
    OMEGA_H_CHECK(mesh.ask_down(2, 0).ab2b == LOs({0, 1, 2, 3}));
    OMEGA_H_CHECK(mesh.ask_down(1, 0).ab2b == LOs({0, 1, 3, 0, 1, 2, 2, 3}));
    OMEGA_H_CHECK(mesh.ask_down(2, 1).ab2b == LOs({0, 2, 3, 1}));
  }
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_HYPERCUBE, 3, LOs(8, 0, 1), 8);
    OMEGA_H_CHECK(mesh.ask_down(3, 0).ab2b == LOs(8, 0, 1));
    OMEGA_H_CHECK(
        mesh.ask_down(2, 0).ab2b == LOs({0, 3, 2, 1, 0, 1, 5, 4, 3, 0, 4, 7, 1,
                                        2, 6, 5, 2, 3, 7, 6, 4, 5, 6, 7}));
    OMEGA_H_CHECK(
        mesh.ask_up(0, 2).ab2b == LOs({0, 1, 2, 0, 1, 3, 0, 3, 4, 0, 2, 4, 1, 2,
                                      5, 1, 3, 5, 3, 4, 5, 2, 4, 5}));
  }
  {
    auto mesh =
        build_box(lib->world(), OMEGA_H_HYPERCUBE, 1.0, 1.0, 1.0, 2, 2, 2);
  }
}

static void test_star(Library* lib) {
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2}), 3);
    Adj v2v = mesh.ask_star(VERT);
    OMEGA_H_CHECK(v2v.a2ab == LOs(4, 0, 2));
    OMEGA_H_CHECK(v2v.ab2b == LOs({1, 2, 0, 2, 0, 1}));
    Adj e2e = mesh.ask_star(EDGE);
    OMEGA_H_CHECK(e2e.a2ab == LOs(4, 0, 2));
    OMEGA_H_CHECK(e2e.ab2b == LOs({2, 1, 0, 2, 1, 0}));
  }
  {
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    Adj v2v = mesh.ask_star(VERT);
    OMEGA_H_CHECK(v2v.a2ab == LOs(5, 0, 3));
    OMEGA_H_CHECK(v2v.ab2b == LOs({1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2}));
    Adj e2e = mesh.ask_star(EDGE);
    OMEGA_H_CHECK(e2e.a2ab == LOs(7, 0, 5));
    OMEGA_H_CHECK(
        e2e.ab2b == LOs({1, 3, 4, 2, 5, 3, 0, 2, 5, 4, 0, 4, 5, 1, 3, 0, 1, 5,
                        4, 2, 2, 0, 3, 5, 1, 1, 2, 4, 3, 0}));
  }
}

static void test_injective_map() {
  LOs primes2ints({2, 3, 5, 7});
  LOs ints2primes = invert_injective_map(primes2ints, 8);
  OMEGA_H_CHECK(ints2primes == LOs({-1, -1, 0, 1, -1, 2, -1, 3}));
}

static void test_dual(Library* lib) {
  Mesh mesh(lib);
  build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2, 2, 3, 0}), 4);
  auto t2t = mesh.ask_dual();
  auto t2tt = t2t.a2ab;
  auto tt2t = t2t.ab2b;
  OMEGA_H_CHECK(t2tt == offset_scan(LOs({1, 1})));
  OMEGA_H_CHECK(tt2t == LOs({1, 0}));
}

static void test_quality() {
  Few<Vector<2>, 3> perfect_tri(
      {vector_2(1, 0), vector_2(0, std::sqrt(3.0)), vector_2(-1, 0)});
  Few<Vector<3>, 4> perfect_tet({vector_3(1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(-1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(0, -1, 1.0 / std::sqrt(2.0)),
      vector_3(0, 1, 1.0 / std::sqrt(2.0))});
  Few<Vector<2>, 3> flat_tri({vector_2(1, 0), vector_2(0, 0), vector_2(-1, 0)});
  Few<Vector<3>, 4> flat_tet({vector_3(1, 0, 0), vector_3(-1, 0, 0),
      vector_3(0, -1, 0), vector_3(0, 1, 0)});
  Few<Vector<2>, 3> inv_tri(
      {vector_2(1, 0), vector_2(-1, 0), vector_2(0, std::sqrt(3.0))});
  Few<Vector<3>, 4> inv_tet({vector_3(1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(-1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(0, 1, 1.0 / std::sqrt(2.0)),
      vector_3(0, -1, 1.0 / std::sqrt(2.0))});
  Matrix<2, 2> id_metric_2 = identity_matrix<2, 2>();
  Matrix<3, 3> id_metric_3 = identity_matrix<3, 3>();
  Matrix<2, 2> x_metric_2 =
      compose_metric(identity_matrix<2, 2>(), vector_2(1, 0.5));
  Matrix<3, 3> x_metric_3 =
      compose_metric(identity_matrix<3, 3>(), vector_3(1, 1, 0.5));
  Few<Vector<2>, 3> x_tri;
  for (Int i = 0; i < 3; ++i) {
    x_tri[i] = perfect_tri[i];
    x_tri[i][1] /= 2;
  }
  Few<Vector<3>, 4> x_tet;
  for (Int i = 0; i < 4; ++i) {
    x_tet[i] = perfect_tet[i];
    x_tet[i][2] /= 2;
  }
  OMEGA_H_CHECK(
      are_close(metric_element_quality(perfect_tri, id_metric_2), 1.0));
  OMEGA_H_CHECK(
      are_close(metric_element_quality(perfect_tet, id_metric_3), 1.0));
  OMEGA_H_CHECK(are_close(metric_element_quality(flat_tri, id_metric_2), 0.0));
  OMEGA_H_CHECK(are_close(metric_element_quality(flat_tet, id_metric_3), 0.0));
  OMEGA_H_CHECK(metric_element_quality(inv_tri, id_metric_2) < 0.0);
  OMEGA_H_CHECK(metric_element_quality(inv_tet, id_metric_3) < 0.0);
  OMEGA_H_CHECK(are_close(metric_element_quality(x_tri, x_metric_2), 1.0));
  OMEGA_H_CHECK(are_close(metric_element_quality(x_tet, x_metric_3), 1.0));
}

static void test_file_components() {
  using namespace binary;
  std::stringstream stream;
  std::string s = "foo";
  LO n = 10;
#ifdef OMEGA_H_USE_ZLIB
  bool is_compressed = true;
#else
  bool is_compressed = false;
#endif
  I8 a = 2;
  write_value(stream, a);
  I32 b = 42 * 1000 * 1000;
  write_value(stream, b);
  I64 c = I64(42) * 1000 * 1000 * 1000;
  write_value(stream, c);
  Real d = 4.2;
  write_value(stream, d);
  Read<I8> aa(n, 0, a);
  write_array(stream, aa);
  Read<I32> ab(n, 0, b);
  write_array(stream, ab);
  Read<I64> ac(n, 0, c);
  write_array(stream, ac);
  Read<Real> ad(n, 0, d);
  write_array(stream, ad);
  write(stream, s);
  I8 a2;
  read_value(stream, a2);
  OMEGA_H_CHECK(a == a2);
  I32 b2;
  read_value(stream, b2);
  OMEGA_H_CHECK(b == b2);
  I64 c2;
  read_value(stream, c2);
  OMEGA_H_CHECK(c == c2);
  Real d2;
  read_value(stream, d2);
  OMEGA_H_CHECK(d == d2);
  Read<I8> aa2;
  read_array(stream, aa2, is_compressed);
  OMEGA_H_CHECK(aa2 == aa);
  Read<I32> ab2;
  read_array(stream, ab2, is_compressed);
  OMEGA_H_CHECK(ab2 == ab);
  Read<I64> ac2;
  read_array(stream, ac2, is_compressed);
  OMEGA_H_CHECK(ac2 == ac);
  Read<Real> ad2;
  read_array(stream, ad2, is_compressed);
  OMEGA_H_CHECK(ad2 == ad);
  std::string s2;
  read(stream, s2);
  OMEGA_H_CHECK(s == s2);
}

static void test_linpart() {
  GO total = 7;
  I32 comm_size = 2;
  OMEGA_H_CHECK(linear_partition_size(total, comm_size, 0) == 4);
  OMEGA_H_CHECK(linear_partition_size(total, comm_size, 1) == 3);
  Read<GO> globals({6, 5, 4, 3, 2, 1, 0});
  auto remotes = globals_to_linear_owners(globals, total, comm_size);
  OMEGA_H_CHECK(remotes.ranks == Read<I32>({1, 1, 1, 0, 0, 0, 0}));
  OMEGA_H_CHECK(remotes.idxs == Read<I32>({2, 1, 0, 3, 2, 1, 0}));
}

static void test_expand() {
  auto fan = offset_scan(LOs({2, 1, 3}));
  Reals data({2.2, 3.14, 42.0});
  OMEGA_H_CHECK(
      expand(data, fan, 1) == Reals({2.2, 2.2, 3.14, 42.0, 42.0, 42.0}));
}

static void test_inertial_bisect(Library* lib) {
  Reals coords({2, 1, 0, 2, -1, 0, -2, 1, 0, -2, -1, 0});
  Reals masses(4, 1);
  auto self = lib->self();
  Real tolerance = 0.0;
  Vector<3> axis;
  auto marked = inertia::mark_bisection(self, coords, masses, tolerance, axis);
  OMEGA_H_CHECK(marked == Read<I8>({1, 1, 0, 0}));
  marked = inertia::mark_bisection_given_axis(
      self, coords, masses, tolerance, vector_3(0, 1, 0));
  OMEGA_H_CHECK(marked == Read<I8>({1, 0, 1, 0}));
}

static void test_average_field(Library* lib) {
  auto mesh = Mesh(lib);
  build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1, 1, 0, 1, 1, 0);
  Reals v2x({2, 1, 3, 2});
  auto e2x = average_field(&mesh, 2, LOs({0, 1}), 1, v2x);
  OMEGA_H_CHECK(are_close(e2x, Reals({5. / 3., 7. / 3.})));
}

template <Int n>
static void test_positivize(Vector<n> pos) {
  auto neg = pos * -1.0;
  OMEGA_H_CHECK(are_close(positivize(pos), pos));
  OMEGA_H_CHECK(are_close(positivize(neg), pos));
}

static void test_positivize() {
  test_positivize(vector_3(1, 1, 1));
  test_positivize(vector_3(1, -1, 1));
  test_positivize(vector_2(-1, 1));
  test_positivize(vector_2(1, 1));
}

static void test_edge_length() {
  OMEGA_H_CHECK(are_close(1., edge_length(1., 1.)));
  OMEGA_H_CHECK(edge_length(1., 2.) > 1.);
  OMEGA_H_CHECK(edge_length(1., 2.) < 1.5);
}

static void test_refine_qualities(Library* lib) {
  auto mesh = Mesh(lib);
  build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1., 1., 0., 1, 1, 0);
  LOs candidates(mesh.nedges(), 0, 1);
  mesh.add_tag(VERT, "metric", symm_ncomps(2),
      repeat_symm(mesh.nverts(), identity_matrix<2, 2>()));
  auto quals = refine_qualities(&mesh, candidates);
  OMEGA_H_CHECK(are_close(
      quals, Reals({0.494872, 0.494872, 0.866025, 0.494872, 0.494872}), 1e-4));
}

static void test_mark_up_down(Library* lib) {
  auto mesh = Mesh(lib);
  build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1., 1., 0., 1, 1, 0);
  OMEGA_H_CHECK(
      mark_down(&mesh, FACE, VERT, Read<I8>({1, 0})) == Read<I8>({1, 1, 0, 1}));
  OMEGA_H_CHECK(
      mark_up(&mesh, VERT, FACE, Read<I8>({0, 1, 0, 0})) == Read<I8>({1, 0}));
}

static void test_compare_meshes(Library* lib) {
  auto a = build_box(lib->world(), OMEGA_H_SIMPLEX, 1., 1., 0., 4, 4, 0);
  OMEGA_H_CHECK(a == a);
  Mesh b = a;
  OMEGA_H_CHECK(a == b);
  b.add_tag<I8>(VERT, "foo", 1, Read<I8>(b.nverts(), 1));
  OMEGA_H_CHECK(!(a == b));
}

static void test_swap2d_topology(Library* lib) {
  auto mesh = Mesh(lib);
  build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1., 1., 0., 1, 1, 0);
  HostFew<LOs, 3> keys2prods;
  HostFew<LOs, 3> prod_verts2verts;
  auto keys2edges = LOs({2});
  swap2d_topology(&mesh, keys2edges, &keys2prods, &prod_verts2verts);
  OMEGA_H_CHECK(prod_verts2verts[EDGE] == LOs({2, 1}));
  OMEGA_H_CHECK(prod_verts2verts[FACE] == LOs({3, 2, 1, 0, 1, 2}));
  OMEGA_H_CHECK(keys2prods[EDGE] == offset_scan(LOs({1})));
  OMEGA_H_CHECK(keys2prods[FACE] == offset_scan(LOs({2})));
}

static void test_swap3d_loop(Library* lib) {
  auto mesh = Mesh(lib);
  build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1, 1, 1, 1, 1, 1);
  auto edges2tets = mesh.ask_up(EDGE, REGION);
  auto edges2edge_tets = edges2tets.a2ab;
  auto edge_tets2tets = edges2tets.ab2b;
  auto edge_tet_codes = edges2tets.codes;
  auto edge_verts2verts = mesh.ask_verts_of(EDGE);
  auto tet_verts2verts = mesh.ask_verts_of(REGION);
  auto f = OMEGA_H_LAMBDA(LO foo) {
    (void)foo;
    LO edge = 6;
    auto loop = swap3d::find_loop(edges2edge_tets, edge_tets2tets,
        edge_tet_codes, edge_verts2verts, tet_verts2verts, edge);
    OMEGA_H_CHECK(loop.eev2v[0] == 7);
    OMEGA_H_CHECK(loop.eev2v[1] == 0);
    OMEGA_H_CHECK(loop.size == 6);
    LO const expect[6] = {2, 3, 1, 5, 4, 6};
    for (Int i = 0; i < loop.size; ++i)
      OMEGA_H_CHECK(loop.loop_verts2verts[i] == expect[i]);
  };
  parallel_for(LO(1), f);
}

static void build_empty_mesh(Mesh* mesh, Int dim) {
  build_from_elems_and_coords(mesh, OMEGA_H_SIMPLEX, dim, LOs({}), Reals({}));
}

static void test_file(Library* lib, Mesh* mesh0) {
  std::stringstream stream;
  binary::write(stream, mesh0);
  Mesh mesh1(lib);
  mesh1.set_comm(lib->self());
  binary::read(stream, &mesh1, binary::latest_version);
  mesh1.set_comm(lib->world());
  auto opts = MeshCompareOpts::init(mesh0, VarCompareOpts::zero_tolerance());
  compare_meshes(mesh0, &mesh1, opts, true, true);
  OMEGA_H_CHECK(*mesh0 == mesh1);
}

static void test_file(Library* lib) {
  {
    auto mesh0 = build_box(lib->world(), OMEGA_H_SIMPLEX, 1., 1., 1., 1, 1, 1);
    test_file(lib, &mesh0);
  }
  {
    Mesh mesh0(lib);
    build_empty_mesh(&mesh0, 3);
    test_file(lib, &mesh0);
  }
}

static void test_xml() {
  xml::Tag tag;
  OMEGA_H_CHECK(!xml::parse_tag("AQAAAAAAAADABg", &tag));
  OMEGA_H_CHECK(!xml::parse_tag("   <Foo bar=\"qu", &tag));
  OMEGA_H_CHECK(!xml::parse_tag("   <Foo bar=", &tag));
  OMEGA_H_CHECK(xml::parse_tag("   <Foo bar=\"quux\"   >", &tag));
  OMEGA_H_CHECK(tag.elem_name == "Foo");
  OMEGA_H_CHECK(tag.attribs["bar"] == "quux");
  OMEGA_H_CHECK(tag.type == xml::Tag::START);
  OMEGA_H_CHECK(xml::parse_tag("   <Elem att=\"val\"  answer=\"42\" />", &tag));
  OMEGA_H_CHECK(tag.elem_name == "Elem");
  OMEGA_H_CHECK(tag.attribs["att"] == "val");
  OMEGA_H_CHECK(tag.attribs["answer"] == "42");
  OMEGA_H_CHECK(tag.type == xml::Tag::SELF_CLOSING);
  OMEGA_H_CHECK(xml::parse_tag("</Foo>", &tag));
  OMEGA_H_CHECK(tag.elem_name == "Foo");
  OMEGA_H_CHECK(tag.type == xml::Tag::END);
}

static void test_read_vtu(Mesh* mesh0) {
  std::stringstream stream;
  vtk::write_vtu(stream, mesh0, mesh0->dim(), vtk::get_all_vtk_tags(mesh0));
  Mesh mesh1(mesh0->library());
  vtk::read_vtu(stream, mesh0->comm(), &mesh1);
  auto opts = MeshCompareOpts::init(mesh0, VarCompareOpts::zero_tolerance());
  OMEGA_H_CHECK(
      OMEGA_H_SAME == compare_meshes(mesh0, &mesh1, opts, true, false));
}

static void test_read_vtu(Library* lib) {
  auto mesh0 = build_box(lib->world(), OMEGA_H_SIMPLEX, 1., 1., 1., 1, 1, 1);
  test_read_vtu(&mesh0);
}

static void test_interpolate_metrics() {
  auto a = repeat_symm(
      4, compose_metric(identity_matrix<2, 2>(), vector_2(1.0 / 100.0, 1.0)));
  auto b = repeat_symm(
      4, compose_metric(identity_matrix<2, 2>(), vector_2(1.0, 1.0)));
  auto c = interpolate_between_metrics(4, a, b, 0.0);
  OMEGA_H_CHECK(are_close(a, c));
  c = interpolate_between_metrics(4, a, b, 1.0);
  OMEGA_H_CHECK(are_close(b, c));
}

static void test_element_implied_metric() {
  /* perfect tri with edge lengths = 2 */
  Few<Vector<2>, 3> perfect_tri(
      {vector_2(1, 0), vector_2(0, std::sqrt(3.0)), vector_2(-1, 0)});
  auto afm = element_implied_metric(perfect_tri);
  auto bfm = compose_metric(identity_matrix<2, 2>(), vector_2(2, 2));
  OMEGA_H_CHECK(are_close(afm, bfm));
  /* perfect tet with edge lengths = 2 */
  Few<Vector<3>, 4> perfect_tet({vector_3(1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(-1, 0, -1.0 / std::sqrt(2.0)),
      vector_3(0, -1, 1.0 / std::sqrt(2.0)),
      vector_3(0, 1, 1.0 / std::sqrt(2.0))});
  auto arm = element_implied_metric(perfect_tet);
  auto brm = compose_metric(identity_matrix<3, 3>(), vector_3(2, 2, 2));
  OMEGA_H_CHECK(are_close(arm, brm));
}

template <Int dim>
static void test_recover_hessians_dim(Library* lib) {
  auto one_if_3d = ((dim == 3) ? 1 : 0);
  auto mesh = build_box(
      lib->world(), OMEGA_H_SIMPLEX, 1., 1., one_if_3d, 4, 4, 4 * one_if_3d);
  auto u_w = Write<Real>(mesh.nverts());
  auto coords = mesh.coords();
  // attach a field = x^2 + y^2 (+ z^2)
  auto f = OMEGA_H_LAMBDA(LO v) {
    auto x = get_vector<dim>(coords, v);
    u_w[v] = norm_squared(x);
  };
  parallel_for(mesh.nverts(), f);
  auto u = Omega_h::Reals(u_w);
  mesh.add_tag(Omega_h::VERT, "u", 1, u);
  auto hess = recover_hessians(&mesh, u);
  // its second derivative is exactly 2dx + 2dy,
  // and both recovery steps are linear so the current
  // algorithm should get an exact answer
  Vector<dim> dv;
  for (Int i = 0; i < dim; ++i) dv[i] = 2;
  auto expected_hess = repeat_symm(mesh.nverts(), diagonal(dv));
  OMEGA_H_CHECK(are_close(hess, expected_hess));
}

static void test_recover_hessians(Library* lib) {
  test_recover_hessians_dim<2>(lib);
  test_recover_hessians_dim<3>(lib);
}

template <Int dim>
static void test_sf_scale_dim(Library* lib) {
  auto nl = 2;
  Int one_if_2d = ((dim >= 2) ? 1 : 0);
  Int one_if_3d = ((dim >= 3) ? 1 : 0);
  auto mesh = build_box(lib->world(), OMEGA_H_SIMPLEX, 1, one_if_2d, one_if_3d,
      nl, nl * one_if_2d, nl * one_if_3d);
  auto target_nelems = mesh.nelems();
  auto metrics = Omega_h::get_implied_metrics(&mesh);
  {
    // auto isos = apply_isotropy(mesh.nverts(), metrics, OMEGA_H_ISO_SIZE);
    auto isos = Omega_h::get_implied_isos(&mesh);
    auto iso_scal = get_metric_scalar_for_nelems(&mesh, isos, target_nelems);
    OMEGA_H_CHECK(are_close(iso_scal, 1.0));
  }
  {
    auto aniso_scal =
        get_metric_scalar_for_nelems(&mesh, metrics, target_nelems);
    OMEGA_H_CHECK(are_close(aniso_scal, 1.0));
  }
}

static void test_sf_scale(Library* lib) {
  test_sf_scale_dim<1>(lib);
  test_sf_scale_dim<2>(lib);
  test_sf_scale_dim<3>(lib);
}

static void test_circumcenter() {
  Few<Vector<3>, 3> right_tri(
      {vector_3(0, 0, 0), vector_3(1, 0, 0), vector_3(0, 1, 0)});
  auto v0 = get_circumcenter_vector(simplex_basis<3, 2>(right_tri));
  OMEGA_H_CHECK(are_close(v0, vector_3(0.5, 0.5, 0)));
  Few<Vector<3>, 3> equal_tri(
      {vector_3(0, std::sqrt(3), 0), vector_3(-1, 0, 0), vector_3(1, 0, 0)});
  auto v1 = get_circumcenter_vector(simplex_basis<3, 2>(equal_tri));
  OMEGA_H_CHECK(are_close(v1, vector_3(0, -std::sqrt(3) * 2.0 / 3.0, 0)));
}

static void test_lie() {
  auto a = identity_matrix<3, 3>();
  auto log_a = log_glp(a);
  auto a2 = exp_glp(log_a);
  OMEGA_H_CHECK(are_close(a2, a));
}

static void test_proximity(Library* lib) {
  {  // triangle with one bridge
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2}), 3);
    mesh.add_tag(VERT, "coordinates", 2, Reals({0, 0, 1, 0, 0, 1}));
    auto dists = get_pad_dists(&mesh, 2, Read<I8>({0, 1, 0}));
    OMEGA_H_CHECK(dists == Reals({-1.0}));
  }
  {  // triangle off-center
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2}), 3);
    mesh.add_tag(VERT, "coordinates", 2, Reals({0, 0, 1, 1, 1, 2}));
    auto dists = get_pad_dists(&mesh, 2, Read<I8>({1, 1, 0}));
    OMEGA_H_CHECK(dists == Reals({-1.0}));
  }
  {  // triangle expected
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 2, LOs({0, 1, 2}), 3);
    mesh.add_tag(VERT, "coordinates", 2, Reals({0, 0, 1, -1, 1, 1}));
    auto dists = get_pad_dists(&mesh, 2, Read<I8>({1, 1, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({1.0})));
  }
  {  // tet with two bridges
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    mesh.add_tag(VERT, "coordinates", 3, Reals(3 * 4, 0.0));
    auto dists = get_pad_dists(&mesh, 3, Read<I8>({1, 1, 0, 0, 0, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({-1.0})));
  }
  {  // tet with three bridges, off-center
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    mesh.add_tag(
        VERT, "coordinates", 3, Reals({0, 0, 0, 1, -1, 1, 1, 1, 1, 1, 0, 2}));
    auto dists = get_pad_dists(&mesh, 3, Read<I8>({1, 1, 1, 0, 0, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({-1.0})));
  }
  {  // tet with three bridges, expected
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    mesh.add_tag(
        VERT, "coordinates", 3, Reals({0, 0, 0, 1, -1, -1, 1, 1, -1, 1, 0, 2}));
    auto dists = get_pad_dists(&mesh, 3, Read<I8>({1, 1, 1, 0, 0, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({1.0})));
  }
  {  // edge-edge tet, off center
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    mesh.add_tag(
        VERT, "coordinates", 3, Reals({0, 0, 0, 1, 0, 0, -1, 1, 0, -1, 1, 1}));
    auto dists = get_pad_dists(&mesh, 3, Read<I8>({0, 1, 1, 1, 1, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({-1.0})));
  }
  {  // edge-edge tet, expected
    Mesh mesh(lib);
    build_from_elems2verts(&mesh, OMEGA_H_SIMPLEX, 3, LOs({0, 1, 2, 3}), 4);
    mesh.add_tag(
        VERT, "coordinates", 3, Reals({0, 0, 0, 2, 0, 0, 1, 1, -1, 1, 1, 1}));
    auto dists = get_pad_dists(&mesh, 3, Read<I8>({0, 1, 1, 1, 1, 0}));
    OMEGA_H_CHECK(are_close(dists, Reals({1.0})));
  }
}

static void test_find_last() {
  auto a = LOs({0, 3, 55, 12});
  OMEGA_H_CHECK(find_last(a, 98) < 0);
  OMEGA_H_CHECK(find_last(a, 12) == 3);
  OMEGA_H_CHECK(find_last(a, 55) == 2);
  OMEGA_H_CHECK(find_last(a, 3) == 1);
  OMEGA_H_CHECK(find_last(a, 0) == 0);
}

static void test_inball() {
  Few<Vector<1>, 2> regular_edge = {{-1.0}, {1.0}};
  auto inball1 = get_inball(regular_edge);
  OMEGA_H_CHECK(are_close(inball1.c, vector_1(0.0)));
  OMEGA_H_CHECK(are_close(inball1.r, 1.0));
  Few<Vector<2>, 3> regular_tri = {
      {-1.0, 0.0}, {1.0, 0.0}, {0.0, std::sqrt(3.0)}};
  auto inball2 = get_inball(regular_tri);
  OMEGA_H_CHECK(are_close(inball2.c, vector_2(0.0, std::sqrt(3.0) / 3.0)));
  OMEGA_H_CHECK(are_close(inball2.r, std::sqrt(3.0) / 3.0));
  Few<Vector<3>, 4> regular_tet = {{1, 0, -1.0 / std::sqrt(2.0)},
      {-1, 0, -1.0 / std::sqrt(2.0)}, {0, -1, 1.0 / std::sqrt(2.0)},
      {0, 1, 1.0 / std::sqrt(2.0)}};
  auto inball3 = get_inball(regular_tet);
  OMEGA_H_CHECK(are_close(inball3.c, vector_3(0.0, 0.0, 0.0)));
  OMEGA_H_CHECK(are_close(inball3.r, 2.0 / std::sqrt(24.0)));
}

static void test_volume_vert_gradients() {
  {
    Few<Vector<1>, 2> parent_edge = {{0.0}, {1.0}};
    auto grad0 = get_size_gradient(parent_edge, 0);
    OMEGA_H_CHECK(are_close(grad0, vector_1(-1.0)));
    auto grad1 = get_size_gradient(parent_edge, 1);
    OMEGA_H_CHECK(are_close(grad1, vector_1(1.0)));
  }
  {
    Few<Vector<2>, 3> parent_tri = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    auto grad0 = get_size_gradient(parent_tri, 0);
    OMEGA_H_CHECK(are_close(grad0, vector_2(-0.5, -0.5)));
    auto grad1 = get_size_gradient(parent_tri, 1);
    OMEGA_H_CHECK(are_close(grad1, vector_2(0.5, 0.0)));
    auto grad2 = get_size_gradient(parent_tri, 2);
    OMEGA_H_CHECK(are_close(grad2, vector_2(0.0, 0.5)));
  }
  {
    Few<Vector<3>, 4> parent_tet = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    auto os = 1.0 / 6.0;
    auto grad0 = get_size_gradient(parent_tet, 0);
    OMEGA_H_CHECK(are_close(grad0, vector_3(-os, -os, -os)));
    auto grad1 = get_size_gradient(parent_tet, 1);
    OMEGA_H_CHECK(are_close(grad1, vector_3(os, 0, 0)));
    auto grad2 = get_size_gradient(parent_tet, 2);
    OMEGA_H_CHECK(are_close(grad2, vector_3(0, os, 0)));
    auto grad3 = get_size_gradient(parent_tet, 3);
    OMEGA_H_CHECK(are_close(grad3, vector_3(0, 0, os)));
  }
}

static void test_1d_box(Library* lib) {
  auto mesh = build_box(lib->world(), OMEGA_H_SIMPLEX, 1, 0, 0, 4, 0, 0);
}

static void test_binary_search(LOs a, LO val, LO expect) {
  auto size = a.size();
  auto f = OMEGA_H_LAMBDA(LO) {
    auto res = binary_search(a, val, size);
    OMEGA_H_CHECK(res == expect);
  };
  parallel_for(1, f);
}

static void test_binary_search() {
  auto a = LOs({20, 30, 40, 50, 90, 100});
  for (LO i = 0; i < a.size(); ++i) test_binary_search(a, a.get(i), i);
  test_binary_search(a, 44, -1);
  test_binary_search(a, 15, -1);
  test_binary_search(a, 10000, -1);
}

static void test_scalar_ptr() {
  Vector<2> v;
  OMEGA_H_CHECK(scalar_ptr(v) == &v[0]);
  auto const& v2 = v;
  OMEGA_H_CHECK(scalar_ptr(v2) == &v2[0]);
  OMEGA_H_CHECK(scalar_ptr(v2) == &v2(0));
  Matrix<5, 4> m;
  OMEGA_H_CHECK(scalar_ptr(m) == &m[0][0]);
  auto const& m2 = m;
  OMEGA_H_CHECK(scalar_ptr(m2) == &m2[0][0]);
  OMEGA_H_CHECK(scalar_ptr(m2) == &m2(0, 0));
}

static void test_is_sorted() {
  OMEGA_H_CHECK(is_sorted(LOs({})));
  OMEGA_H_CHECK(is_sorted(Reals({42.0})));
  OMEGA_H_CHECK(is_sorted(LOs({0, 1, 2})));
  OMEGA_H_CHECK(!is_sorted(Reals({0.2, 0.1, 0.3, 0.4})));
  OMEGA_H_CHECK(is_sorted(Reals({0.1, 0.1, 0.1, 0.1})));
}

static void test_expr() {
#ifdef OMEGA_H_USE_TEUCHOSPARSER
  using Teuchos::any;
  using Teuchos::any_cast;
  auto reader = ExprReader{4, 3};
  any result;
  reader.read_string(result, "1.0", "test0");
  OMEGA_H_CHECK(any_cast<Real>(result) == 1.0);
  reader.read_string(result, "1 + 1", "test1");
  OMEGA_H_CHECK(any_cast<Real>(result) == 2.0);
  reader.register_variable("pi", any(Real(3.14159)));
  reader.read_string(result, "pi", "test2");
  OMEGA_H_CHECK(any_cast<Real>(result) == 3.14159);
  reader.register_variable("j", any(vector_3(0, 1, 0)));
  reader.read_string(result, "pi * j", "test3");
  OMEGA_H_CHECK(
      are_close(any_cast<Vector<3>>(result), vector_3(0, 3.14159, 0)));
  reader.register_variable("x", any(Reals({0, 1, 2, 3})));
  reader.read_string(result, "x^2", "test4");
  OMEGA_H_CHECK(are_close(any_cast<Reals>(result), Reals({0, 1, 4, 9})));
  reader.register_variable(
      "v", any(Reals({0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0})));
  reader.read_string(result, "v(1)", "test5");
  OMEGA_H_CHECK(are_close(any_cast<Reals>(result), Reals({0, 1, 2, 3})));
  reader.read_string(result, "v - 1.5 * j", "test6");
  OMEGA_H_CHECK(are_close(any_cast<Reals>(result),
      Reals({0, -1.5, 0, 0, -0.5, 0, 0, 0.5, 0, 0, 1.5, 0})));
  reader.read_string(result, "vector(0, 1, 2)", "test7");
  OMEGA_H_CHECK(are_close(any_cast<Vector<3>>(result), vector_3(0, 1, 2)));
  reader.read_string(result, "vector(x, 0, 0)", "test8");
  OMEGA_H_CHECK(are_close(
      any_cast<Reals>(result), Reals({0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0})));
  reader.read_string(result, "exp(0)", "test9");
  OMEGA_H_CHECK(are_close(any_cast<Real>(result), 1.0));
  reader.read_string(result, "exp(x)", "test10");
  OMEGA_H_CHECK(are_close(any_cast<Reals>(result),
      Reals({1.0, std::exp(1.0), std::exp(2.0), std::exp(3.0)})));
#endif
}

static void test_sort_small_range() {
  Read<I32> in({10, 100, 1000, 10, 100, 1000, 10, 100, 1000});
  LOs perm;
  LOs fan;
  Read<I32> uniq;
  sort_small_range(in, &perm, &fan, &uniq);
  OMEGA_H_CHECK(perm == LOs({0, 3, 6, 1, 4, 7, 2, 5, 8}));
  OMEGA_H_CHECK(fan == LOs({0, 3, 6, 9}));
  OMEGA_H_CHECK(uniq == Read<I32>({10, 100, 1000}));
  in = Read<I32>({});
  sort_small_range(in, &perm, &fan, &uniq);
  OMEGA_H_CHECK(perm == LOs({}));
  OMEGA_H_CHECK(fan == LOs({0}));
  OMEGA_H_CHECK(uniq == Read<I32>({}));
}

static void test_most_normal() {
  {
    Few<Vector<3>, 3> N = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    auto N_c = get_most_normal_normal(N, 3);
    OMEGA_H_CHECK(are_close(N_c, normalize(vector_3(1, 1, 1))));
  }
  {
    Few<Vector<3>, 4> N = {
        {1, 0, 0}, {0, 0, 1}, normalize(vector_3(1, 1, 1)), {0, 1, 0}};
    auto N_c = get_most_normal_normal(N, 4);
    OMEGA_H_CHECK(are_close(N_c, normalize(vector_3(1, 1, 1))));
  }
  {
    Few<Vector<3>, 3> N = {{1, 0, 0}, {0, 1, 0}, {0, 1, 0}};
    auto N_c = get_most_normal_normal(N, 3);
    OMEGA_H_CHECK(are_close(N_c, normalize(vector_3(1, 1, 0))));
  }
  {
    Few<Vector<3>, 3> N = {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}};
    auto N_c = get_most_normal_normal(N, 3);
    OMEGA_H_CHECK(are_close(N_c, normalize(vector_3(1, 0, 0))));
  }
}

int main(int argc, char** argv) {
  auto lib = Library(&argc, &argv);
  OMEGA_H_CHECK(std::string(lib.version()) == OMEGA_H_SEMVER);
  test_edge_length();
  test_power();
  test_cubic();
  test_form_ortho_basis();
  test_qr_decomps();
  test_eigen(matrix_1x1(42.0), vector_1(42.0));
  test_eigen_quadratic();
  test_eigen_cubic();
  test_eigen_jacobi();
  test_least_squares();
  test_int128();
  test_most_normal();
  test_repro_sum();
  test_sort();
  test_sort_small_range();
  test_scan();
  test_intersect_metrics();
  test_fan_and_funnel();
  test_permute();
  test_invert_map();
  test_invert_adj();
  test_tri_align();
  test_form_uses();
  test_reflect_down();
  test_find_unique();
  test_hilbert();
  test_bbox();
  test_build(&lib);
  test_star(&lib);
  test_injective_map();
  test_dual(&lib);
  test_quality();
  test_file_components();
  test_linpart();
  test_expand();
  test_inertial_bisect(&lib);
  test_average_field(&lib);
  test_positivize();
  test_refine_qualities(&lib);
  test_mark_up_down(&lib);
  test_compare_meshes(&lib);
  test_swap2d_topology(&lib);
  test_swap3d_loop(&lib);
  test_file(&lib);
  test_xml();
  test_read_vtu(&lib);
  test_interpolate_metrics();
  test_element_implied_metric();
  test_recover_hessians(&lib);
  test_sf_scale(&lib);
  test_circumcenter();
  test_lie();
  test_proximity(&lib);
  test_find_last();
  test_inball();
  test_volume_vert_gradients();
  test_1d_box(&lib);
  test_binary_search();
  test_scalar_ptr();
  test_is_sorted();
  test_expr();
  OMEGA_H_CHECK(get_current_bytes() == 0);
}
