#include "Omega_h_array_ops.hpp"

#include "Omega_h_loop.hpp"

namespace Omega_h {

template <typename T>
struct SameContent : public AndFunctor {
  Read<T> a_;
  Read<T> b_;
  SameContent(Read<T> a, Read<T> b) : a_(a), b_(b) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = update && (a_[i] == b_[i]);
  }
};

template <typename T>
bool operator==(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  return parallel_reduce(a.size(), SameContent<T>(a, b), "operator==");
}

template <typename T>
struct Sum : public SumFunctor<T> {
  using typename SumFunctor<T>::value_type;
  Read<T> a_;
  Sum(Read<T> a) : a_(a) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = update + a_[i];
  }
};

template <typename T>
typename StandinTraits<T>::type get_sum(Read<T> a) {
  return parallel_reduce(a.size(), Sum<T>(a), "get_sum");
}

template <typename T>
typename StandinTraits<T>::type get_sum(CommPtr comm, Read<T> a) {
  return comm->allreduce(get_sum(a), OMEGA_H_SUM);
}

template <typename T>
struct Min : public MinFunctor<T> {
  using typename MinFunctor<T>::value_type;
  Read<T> a_;
  Min(Read<T> a) : a_(a) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = min2<value_type>(update, a_[i]);
  }
};

template <typename T>
T get_min(Read<T> a) {
  auto r = parallel_reduce(a.size(), Min<T>(a), "get_min");
  return static_cast<T>(r);  // see StandinTraits
}

template <typename T>
struct Max : public MaxFunctor<T> {
  using typename MaxFunctor<T>::value_type;
  Read<T> a_;
  Max(Read<T> a) : a_(a) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = max2<value_type>(update, a_[i]);
  }
};

template <typename T>
T get_max(Read<T> a) {
  auto r = parallel_reduce(a.size(), Max<T>(a), "get_max");
  return static_cast<T>(r);  // see StandinTraits
}

template <typename T>
T get_min(CommPtr comm, Read<T> a) {
  return comm->allreduce(get_min(a), OMEGA_H_MIN);
}

template <typename T>
T get_max(CommPtr comm, Read<T> a) {
  return comm->allreduce(get_max(a), OMEGA_H_MAX);
}

template <typename T>
MinMax<T> get_minmax(CommPtr comm, Read<T> a) {
  return {get_min(comm, a), get_max(comm, a)};
}

struct AreClose : public AndFunctor {
  Reals a_;
  Reals b_;
  Real tol_;
  Real floor_;
  AreClose(Reals a, Reals b, Real tol, Real floor)
      : a_(a), b_(b), tol_(tol), floor_(floor) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = update && are_close(a_[i], b_[i], tol_, floor_);
  }
};

bool are_close(Reals a, Reals b, Real tol, Real floor) {
  OMEGA_H_CHECK(a.size() == b.size());
  auto f = AreClose(a, b, tol, floor);
  auto res = parallel_reduce(a.size(), f, "are_close");
  return static_cast<bool>(res);
}

struct AreCloseAbs : public AndFunctor {
  Reals a_;
  Reals b_;
  Real tol_;
  AreCloseAbs(Reals a, Reals b, Real tol) : a_(a), b_(b), tol_(tol) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = update && (std::abs(a_[i] - b_[i]) <= tol_);
  }
};

bool are_close_abs(Reals a, Reals b, Real tol) {
  OMEGA_H_CHECK(a.size() == b.size());
  auto f = AreCloseAbs(a, b, tol);
  auto res = parallel_reduce(a.size(), f, "are_close_abs");
  return static_cast<bool>(res);
}

template <typename T>
Read<T> multiply_each_by(Read<T> a, T b) {
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] * b; };
  parallel_for(a.size(), f, "multiply_each_by");
  return c;
}

template <typename T>
Read<T> multiply_each(Read<T> a, Read<T> b) {
  if (b.size() == 0) {
    OMEGA_H_CHECK(a.size() == 0);
    return a;
  }
  auto width = divide_no_remainder(a.size(), b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) {
    for (Int j = 0; j < width; ++j) {
      c[i * width + j] = a[i * width + j] * b[i];
    }
  };
  parallel_for(b.size(), f, "multiply_each");
  return c;
}

template <typename T>
Read<T> divide_each(Read<T> a, Read<T> b) {
  if (b.size() == 0) {
    OMEGA_H_CHECK(a.size() == 0);
    return a;
  }
  auto width = divide_no_remainder(a.size(), b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) {
    for (Int j = 0; j < width; ++j) {
      c[i * width + j] = a[i * width + j] / b[i];
    }
  };
  parallel_for(b.size(), f, "divide_each");
  return c;
}

Reals divide_each_maybe_zero(Reals a, Reals b) {
  if (b.size() == 0) {
    OMEGA_H_CHECK(a.size() == 0);
    return a;
  }
  auto width = divide_no_remainder(a.size(), b.size());
  Write<Real> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) {
    if (b[i] != 0.0) {
      for (Int j = 0; j < width; ++j) {
        c[i * width + j] = a[i * width + j] / b[i];
      }
    } else {
      for (Int j = 0; j < width; ++j) {
        OMEGA_H_CHECK(a[i * width + j] == 0.0);
        c[i * width + j] = 0.0;
      }
    }
  };
  parallel_for(b.size(), f, "divide_maybe_zero");
  return c;
}

Reals pow_each(Reals a, Reals b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<Real> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = std::pow(a[i], b[i]); };
  parallel_for(a.size(), f, "pow_each");
  return c;
}

template <typename T>
Read<T> divide_each_by(Read<T> a, T b) {
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] / b; };
  parallel_for(a.size(), f, "divide_each_by");
  return c;
}

template <typename T>
Read<T> add_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] + b[i]; };
  parallel_for(c.size(), f, "add_each");
  return c;
}

template <typename T>
Read<T> subtract_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] - b[i]; };
  parallel_for(c.size(), f, "subtract_each");
  return c;
}

template <typename T>
Read<T> add_to_each(Read<T> a, T b) {
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] + b; };
  parallel_for(c.size(), f, "add_to_each");
  return c;
}

template <typename T>
Read<T> subtract_from_each(Read<T> a, T b) {
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = a[i] - b; };
  parallel_for(c.size(), f, "subtract_from_each");
  return c;
}

template <typename T>
Bytes each_geq_to(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] >= b); };
  parallel_for(c.size(), f, "each_geq_to");
  return c;
}

template <typename T>
Bytes each_leq_to(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] <= b); };
  parallel_for(c.size(), f, "each_leq_to");
  return c;
}

template <typename T>
Bytes each_gt(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] > b); };
  parallel_for(c.size(), f, "each_gt");
  return c;
}

template <typename T>
Bytes each_lt(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] < b); };
  parallel_for(c.size(), f, "each_lt");
  return c;
}

template <typename T>
Bytes gt_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] > b[i]); };
  parallel_for(c.size(), f, "gt_each");
  return c;
}

template <typename T>
Bytes lt_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] < b[i]); };
  parallel_for(c.size(), f, "lt_each");
  return c;
}

template <typename T>
Bytes eq_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] == b[i]); };
  parallel_for(c.size(), f, "eq_each");
  return c;
}

template <typename T>
Bytes geq_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] >= b[i]); };
  parallel_for(c.size(), f, "geq_each");
  return c;
}

template <typename T>
Read<T> min_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = min2(a[i], b[i]); };
  parallel_for(c.size(), f, "min_each");
  return c;
}

template <typename T>
Read<T> max_each(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = max2(a[i], b[i]); };
  parallel_for(c.size(), f, "max_each");
  return c;
}

template <typename T>
Read<T> ternary_each(Bytes cond, Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  auto width = divide_no_remainder(a.size(), cond.size());
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = cond[i / width] ? a[i] : b[i]; };
  parallel_for(c.size(), f, "ternary_each");
  return c;
}

template <typename T>
Read<T> each_max_with(Read<T> a, T b) {
  Write<T> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = max2(a[i], b); };
  parallel_for(c.size(), f, "each_max_with");
  return c;
}

template <typename T>
Bytes each_neq_to(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] != b); };
  parallel_for(c.size(), f, "each_neq_to");
  return c;
}

template <typename T>
Bytes each_eq(Read<T> a, Read<T> b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] == b[i]); };
  parallel_for(c.size(), f, "each_eq");
  return c;
}

template <typename T>
Bytes each_eq_to(Read<T> a, T b) {
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] == b); };
  parallel_for(c.size(), f, "each_eq_to");
  return c;
}

Bytes land_each(Bytes a, Bytes b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] && b[i]); };
  parallel_for(c.size(), f, "land_each");
  return c;
}

Bytes lor_each(Bytes a, Bytes b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] || b[i]); };
  parallel_for(c.size(), f, "lor_each");
  return c;
}

Bytes bit_or_each(Bytes a, Bytes b) {
  OMEGA_H_CHECK(a.size() == b.size());
  Write<I8> c(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { c[i] = (a[i] | b[i]); };
  parallel_for(c.size(), f, "bit_or_each");
  return c;
}

Bytes bit_neg_each(Bytes a) {
  Write<I8> b(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { b[i] = ~(a[i]); };
  parallel_for(a.size(), f, "bit_neg_each");
  return b;
}

Read<Real> fabs_each(Read<Real> a) {
  Write<Real> b(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { b[i] = std::abs(a[i]); };
  parallel_for(a.size(), f, "fabs_each");
  return b;
}

template <typename T>
Read<T> get_component(Read<T> a, Int ncomps, Int comp) {
  Write<T> b(divide_no_remainder(a.size(), ncomps));
  auto f = OMEGA_H_LAMBDA(LO i) { b[i] = a[i * ncomps + comp]; };
  parallel_for(b.size(), f, "get_component");
  return b;
}

template <typename T>
void set_component(Write<T> out, Read<T> a, Int ncomps, Int comp) {
  auto f = OMEGA_H_LAMBDA(LO i) { out[i * ncomps + comp] = a[i]; };
  parallel_for(a.size(), f, "set_component");
}

template <typename T>
struct FindLast : public MaxFunctor<LO> {
  using typename MaxFunctor<LO>::value_type;
  Read<T> array_;
  T value_;
  FindLast(Read<T> array, T value) : array_(array), value_(value) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    if (array_[i] == value_) {
      update = max2<value_type>(update, i);
    }
  }
};

template <typename T>
LO find_last(Read<T> array, T value) {
  auto f = FindLast<T>(array, value);
  auto res = parallel_reduce(array.size(), f, "find_last");
  return (res == ArithTraits<LO>::min()) ? -1 : res;
}

template <typename T>
struct IsSorted : public AndFunctor {
  Read<T> a_;
  IsSorted(Read<T> a) : a_(a) {}
  OMEGA_H_DEVICE void operator()(LO i, value_type& update) const {
    update = update && (a_[i] <= a_[i + 1]);
  }
};

template <typename T>
bool is_sorted(Read<T> a) {
  if (a.size() < 2) return true;
  return parallel_reduce(a.size() - 1, IsSorted<T>(a), "is_sorted");
}

template <typename T>
Read<T> interleave(std::vector<Read<T>> arrays) {
  if (arrays.empty()) return Read<T>();
  auto narrays = LO(arrays.size());
  auto array_size = arrays.front().size();
  for (auto& array : arrays) OMEGA_H_CHECK(array.size() == array_size);
  auto out_size = narrays * array_size;
  auto out = Write<T>(out_size);
  for (LO i = 0; i < narrays; ++i) {
    auto array = arrays[std::size_t(i)];
    auto f = OMEGA_H_LAMBDA(LO j) { out[j * narrays + i] = array[j]; };
    parallel_for(array_size, f, "interleave");
  }
  return out;
}

/* A reproducible sum of floating-point values.
   this operation is one of the key places where
   a program's output begins to depend on parallel
   partitioning and traversal order, because
   floating-point values do not produce the same
   sum when added in a different order.

   IEEE 754 64-bit floating point format is assumed,
   which has 52 bits in the fraction.

   The idea here is to add the numbers as fixed-point values.
   max_exponent() finds the largest exponent (e) such that
   all values are (<= 2^(e)).
   We then use the value (2^(e - 52)) as the unit, and sum all
   values as integers in that unit.
   This is guaranteed to be at least as accurate as the
   worst-case ordering of the values, i.e. being added
   in order of decreasing magnitude.

   If we used a 64-bit integer type, we would only be
   able to reliably add up to (2^12 = 4096) values
   (64 - 52 = 12).
   Thus we use a 128-bit integer type.
   This allows us to reliably add up to (2^76 > 10^22)
   values. By comparison, supercomputers today
   support a maximum of one million MPI ranks (10^6)
   and each rank typically can't hold more than
   one billion values (10^9), for a total of (10^15) values.
*/

struct MaxExponent : public MaxFunctor<int> {
  Reals a_;
  MaxExponent(Reals a) : a_(a) {}
  OMEGA_H_DEVICE void operator()(Int i, value_type& update) const {
    int expo;
    std::frexp(a_[i], &expo);
    if (expo > update) update = expo;
  }
};

static int max_exponent(Reals a) {
  auto res = parallel_reduce(a.size(), MaxExponent(a), "max_exponent");
  return static_cast<int>(res);
}

struct ReproSum : public SumFunctor<Int128> {
  Reals a_;
  double unit_;
  ReproSum(Reals a, double unit) : a_(a), unit_(unit) {}
  OMEGA_H_DEVICE void operator()(Int i, value_type& update) const {
    update = update + Int128::from_double(a_[i], unit_);
  }
};

Real repro_sum(Reals a) {
  begin_code("repro_sum");
  int expo = max_exponent(a);
  double unit = exp2(double(expo - MANTISSA_BITS));
  Int128 fixpt_sum = parallel_reduce(a.size(), ReproSum(a, unit), "fixpt_sum");
  end_code();
  return fixpt_sum.to_double(unit);
}

Real repro_sum(CommPtr comm, Reals a) {
  begin_code("repro_sum(comm)");
  int expo = comm->allreduce(max_exponent(a), OMEGA_H_MAX);
  double unit = exp2(double(expo - MANTISSA_BITS));
  Int128 fixpt_sum = parallel_reduce(a.size(), ReproSum(a, unit), "fixpt_sum");
  fixpt_sum = comm->add_int128(fixpt_sum);
  end_code();
  return fixpt_sum.to_double(unit);
}

void repro_sum(CommPtr comm, Reals a, Int ncomps, Real result[]) {
  for (Int comp = 0; comp < ncomps; ++comp) {
    result[comp] = repro_sum(comm, get_component(a, ncomps, comp));
  }
}

Reals interpolate_between(Reals a, Reals b, Real t) {
  OMEGA_H_CHECK(a.size() == b.size());
  auto n = a.size();
  auto out = Write<Real>(n);
  auto f = OMEGA_H_LAMBDA(LO i) { out[i] = a[i] * (1.0 - t) + b[i] * t; };
  parallel_for(n, f, "interpolate_between");
  return out;
}

Reals invert_each(Reals a) {
  auto out = Write<Real>(a.size());
  auto f = OMEGA_H_LAMBDA(LO i) { out[i] = 1.0 / a[i]; };
  parallel_for(a.size(), f, "invert_each");
  return out;
}

template <typename Tout, typename Tin>
Read<Tout> array_cast(Read<Tin> in) {
  auto out = Write<Tout>(in.size());
  auto f = OMEGA_H_LAMBDA(LO i) { out[i] = static_cast<Tout>(in[i]); };
  parallel_for(in.size(), f, "array_cast");
  return out;
}

#define INST(T)                                                                \
  template bool operator==(Read<T> a, Read<T> b);                              \
  template typename StandinTraits<T>::type get_sum(Read<T> a);                 \
  template T get_min(Read<T> a);                                               \
  template T get_max(Read<T> a);                                               \
  template typename StandinTraits<T>::type get_sum(CommPtr comm, Read<T> a);   \
  template T get_min(CommPtr comm, Read<T> a);                                 \
  template T get_max(CommPtr comm, Read<T> a);                                 \
  template MinMax<T> get_minmax(CommPtr comm, Read<T> a);                      \
  template Read<T> multiply_each_by(Read<T> a, T b);                           \
  template Read<T> divide_each_by(Read<T> x, T b);                             \
  template Read<T> multiply_each(Read<T> a, Read<T> b);                        \
  template Read<T> divide_each(Read<T> a, Read<T> b);                          \
  template Read<T> add_each(Read<T> a, Read<T> b);                             \
  template Read<T> subtract_each(Read<T> a, Read<T> b);                        \
  template Read<T> min_each(Read<T> a, Read<T> b);                             \
  template Read<T> max_each(Read<T> a, Read<T> b);                             \
  template Read<T> ternary_each(Bytes cond, Read<T> a, Read<T> b);             \
  template Read<T> each_max_with(Read<T> a, T b);                              \
  template Read<T> add_to_each(Read<T> a, T b);                                \
  template Read<T> subtract_from_each(Read<T> a, T b);                         \
  template Bytes each_geq_to(Read<T> a, T b);                                  \
  template Bytes each_leq_to(Read<T> a, T b);                                  \
  template Bytes each_gt(Read<T> a, T b);                                      \
  template Bytes each_lt(Read<T> a, T b);                                      \
  template Bytes each_neq_to(Read<T> a, T b);                                  \
  template Bytes each_eq(Read<T> a, Read<T> b);                                \
  template Bytes each_eq_to(Read<T> a, T b);                                   \
  template Bytes gt_each(Read<T> a, Read<T> b);                                \
  template Bytes lt_each(Read<T> a, Read<T> b);                                \
  template Bytes eq_each(Read<T> a, Read<T> b);                                \
  template Read<T> get_component(Read<T> a, Int ncomps, Int comp);             \
  template void set_component(Write<T> out, Read<T> a, Int ncomps, Int comp);  \
  template LO find_last(Read<T> array, T value);                               \
  template bool is_sorted(Read<T> a);                                          \
  template Read<T> interleave(std::vector<Read<T>> arrays);

INST(I8)
INST(I32)
INST(I64)
INST(Real)
#undef INST

template Read<Real> array_cast(Read<I32>);
template Read<I32> array_cast(Read<I8>);

}  // end namespace Omega_h
