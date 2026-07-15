//@ expect val 118
struct Sieve { u32[64] is_prime  u32 count }
impl Sieve {
    fn init() {
        u32 i = 0
        while i < 64 { self.is_prime[i] = 1  i = i + 1 }
        self.is_prime[0] = 0
        self.is_prime[1] = 0
    }
    fn run() {
        u32 p = 2
        while p * p < 64 {
            if self.is_prime[p] == 1 {
                u32 m = p * p
                while m < 64 { self.is_prime[m] = 0  m = m + p }
            }
            p = p + 1
        }
    }
    fn tally() {
        u32 i = 0
        self.count = 0
        while i < 64 { self.count = self.count + self.is_prime[i]  i = i + 1 }
    }
}
fn build_primes() Sieve {
    Sieve s = {.count = 0}
    s.init()
    s.run()
    s.tally()
    return s
}
// entire sieve runs at compile time; the const holds the result
const Sieve PRIMES = build_primes()
const u32 PRIME_COUNT = PRIMES.count          // 18 primes below 64
const u32 CHECK_7  = PRIMES.is_prime[7]       // 7 is prime -> 1
const u32 CHECK_9  = PRIMES.is_prime[9]       // 9 is composite -> 0
fn main() i32 {
    // 18 + 1*100 + 0 = 118 ; also read the const at runtime
    return (i32)(PRIME_COUNT + CHECK_7 * 100 + CHECK_9)
}
