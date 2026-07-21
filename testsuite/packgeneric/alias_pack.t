//@ expect val 3
// An alias may take a pack too -- it goes through the same shared
// parse_generic_param_list/parse_generic_arg_list_packed pair as struct/fn.
alias Bundle[Ts...] = Ts
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn main() i32 { return (i32)nf[Bundle[i32, u8, f64]]() }
