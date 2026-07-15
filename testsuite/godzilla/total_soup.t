//@ expect val 701
struct Pair[A, B] { A first  B second }
enum Result[T, E] { T Ok  E Err }
struct Processor[T] { fn(T) Result[T, u32] process  T seed }
fn validate(u32 x) Result[u32, u32] {
    if x > 50 { return .Err{x} }
    return .Ok{x * 2}
}
fn run_proc[T](Processor[T]* p) Result[T, u32] { return p.process(p.seed) }
fn main() i32 {
    Processor[u32][3] procs = {
        {.process = validate, .seed = 10},
        {.process = validate, .seed = 60},
        {.process = validate, .seed = 25}
    }
    u32 total = 0
    u32 errors = 0
    for u32 i = 0 to 3 {
        Processor[u32]* pp = &procs[i]
        match run_proc(pp) {
            .Ok{v} { total = total + v }
            .Err{e} { errors = errors + 1 }
        }
    }
    Pair[u32, u32] summary = {.first = total, .second = errors}
    return (i32)(summary.first * 10 + summary.second)
}
