//@ expect err expects 2 type arguments
// An HKT slot bound to a template that needs MORE params than the M[T]
// application supplies (Tensor needs [T, Shape2 S], but M[T] gives one arg)
// must be a clean arity error, not a silent zero-sized struct. This hits a
// different path from arity_mismatch_rejected.t: there, enough brackets but
// one isn't type-shaped; here, fewer brackets than the template's arity, so
// array_substitute_maybe_hkt's type_param_count > depth guard fires -- which
// used to mean "not an HKT, leave alone" and left Tensor unapplied (still
// is_generic), read by Type_SizeOf as 0. The struct_unapplied flag now tells
// a deliberately-unapplied template apart from an ordinary generic struct
// merely named as an array element, so only the former errors.
struct Shape2 { u32 rows  u32 cols }
struct Tensor[T, Shape2 S] { T[S.rows * S.cols] data }
struct HKT[M, T] { M[T] data }
fn main() i32 {
    HKT[Tensor, i32] h
    return (i32)sizeof(h)
}
