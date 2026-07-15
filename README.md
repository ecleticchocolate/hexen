# Hexen

A small systems language. Declarations are type-first (no `let`/`var`), statements
end at the newline, generics can carry values as well as types, and `match` works
the same way whether the scrutinee is a value or a type.

```
extern fn printf(u8* fmt, ...) i32

enum Option[T] { T Some  None }

fn find_first_even(i32[5] xs) Option[i32] {
    for u32 i = 0 to 5 {
        if xs[i] % 2 == 0 { return .Some{xs[i]} }
    }
    return .None
}

fn main() i32 {
    i32[5] nums = {1, 3, 8, 5, 2}
    match find_first_even(nums) {
        .Some{v} { printf("found: %d\n", v) }
        .None    { printf("none\n") }
    }
    return 0
}
```

Two more, since the above is deliberately tame:

**Const-generic dimensions checked against each other at compile time** — a
shape-mismatched matrix multiply is a compile error, not a runtime crash:
```
struct M[T, u32 R, u32 C] { T[R * C] e }

fn matmul[T, u32 R, u32 K, u32 Cc](M[T, R, K] a, M[T, K, Cc] b) M[T, R, Cc] {
    M[T, R, Cc] out
    for u32 i = 0 to R {
        for u32 j = 0 to Cc {
            T sum = (T)0
            for u32 k = 0 to K { sum = sum + a.e[i * K + k] * b.e[k * Cc + j] }
            out.e[i * Cc + j] = sum
        }
    }
    return out
    // K must agree between a's and b's own types, or this call doesn't
    // compile at all -- there's no shape to check at runtime, it's gone.
}
```

**A dispatch function baked into the type itself as a const-generic
parameter** — the "virtual call" is fully resolved at compile time; `sizeof`
proves nothing runtime-shaped (a vtable pointer, a tag) is actually stored:
```
struct Circle { i32 r }
impl Circle { fn area() i32 { return 42 } }
fn t_area[T](void* p) i32 { T* o = (T*)p  return o.area() }
struct DynA[fn(void*) i32 F] { void* obj }
impl DynA[F] { fn area() i32 { return F(self.obj) } }
fn main() i32 {
    Circle c = { .r = 1 }
    DynA[t_area[Circle]] d = { .obj = (void*)&c }
    return (i32)sizeof(d)   // 8 -- just the data pointer, nothing else
}
```

## Build

```
make            # -> ./torrent
make debug      # ASAN/UBSAN build -> ./torrent_debug
./test.sh       # run the regression suite (970 tests)
```

No dependencies beyond `gcc` and `libdl`.

## Run

```
./torrent file.t                    # JIT: compile + run main()
./torrent -c file.t -o out.o        # AOT: emit a relocatable object
gcc -o out aot_shim.c out.o         # link the object into a runnable binary
./torrent a.t b.t                   # multiple files, one compilation
./torrent -emit-mod out.tmod file.t # write a module interface file
```

## What's here

The compiler is ~14 C files: lexer, a hand-written recursive-descent parser,
a type checker, a compile-time AST interpreter (`const` expressions run the
same interpreter as everything else — no restricted subset), and an x86-64
JIT/AOT backend. `testsuite/` is ~970 single-file regression tests, each
declaring its own expected result in a header comment.

## The language, briefly

- **No `let`/`var`.** A declaration is `TYPE name`, everywhere — locals,
  parameters, fields.
- **Generics carry values, not just types.** `struct Vec[T, u32 N] { T[N] e }`
  — `N` is a real compile-time value, usable in field types, array sizes,
  and method bodies.
- **One `match`.** Matching a value compares literals and destructures
  structs/enums/arrays. Matching a *type* does the same thing structurally
  — `match T { Box[E] { ... }  fn(A) B { ... } }` — used for compile-time
  reflection over a type's shape, no separate reflection API.
- **`impl` is a mangler, not a vtable.** `impl Foo { fn m() {...} }` lowers
  to a plain function `Foo_m(Foo* self)`; `x.m()` is call-site sugar. No
  hidden dispatch.
- **`const` runs real code.** A `const` initializer is evaluated by the same
  AST interpreter that would run it at runtime — heap allocation, pointer
  chains, loops, generics, all of it, folded to a value before the binary
  exists.
- **`super` embeds fields**, not behavior: `struct Derived { super Base b  ... }`
  promotes `Base`'s fields into `Derived`, independent-copy semantics.

None of this needs a runtime type system: generics monomorphize, `match`
on a type evaporates at compile time, and `impl` is pure name mangling.

For the full construct-by-construct reference (every keyword, its grammar,
and a minimal verified example, dense enough to build real programs from
alone), see [`docs/REFERENCE.md`](docs/REFERENCE.md) — this README is the
short version.
