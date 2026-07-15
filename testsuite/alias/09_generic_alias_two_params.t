//@ expect val 5
alias Either[A, B] = struct { A first  B second }
fn main() i32 { Either[i32, u8] e = { .first = 3, .second = 2 }  return e.first + (i32)e.second }
