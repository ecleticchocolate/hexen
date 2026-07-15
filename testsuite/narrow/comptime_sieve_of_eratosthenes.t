//@ expect val 25
struct Sieve { bool[100] is_prime }
fn build_primes() Sieve {
    Sieve s
    u32 i = 0
    while i < 100 { s.is_prime[i] = true  i = i + 1 }
    s.is_prime[0] = false
    s.is_prime[1] = false
    u32 p = 2
    while p * p < 100 {
        if s.is_prime[p] {
            u32 m = p * p
            while m < 100 { s.is_prime[m] = false  m = m + p }
        }
        p = p + 1
    }
    return s
}
const Sieve PRIMES = build_primes()
fn main() i32 {
    u32 count = 0
    u32 i = 0
    while i < 100 {
        if PRIMES.is_prime[i] { count = count + 1 }
        i = i + 1
    }
    return (i32)count
}
