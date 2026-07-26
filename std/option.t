// Option sum type for handling potentially absent values

pub enum Option[T] {
    T Some
    None
}

// The one ownership predicate, named once for the whole standard library.
// "Does T own a resource?" is asked by every container that stores a T, and
// every one of them answers it by matching against THIS alias rather than
// respelling the structural query -- so the predicate cannot drift between
// Vector, Array, and whatever comes next.
//
// It lives here because option.t is the one file every container already
// depends on (get()/pop() return Option[T]), so no new dependency edge is
// created by putting it here.
pub alias Owning = impl { fn __delete() }

// The opt-in hash hook. A type that defines __hash() controls its own hashing
// everywhere -- HashMap consults this before any structural fallback, so a type
// that must hash only part of itself (or hash something its fields cannot
// express) says so once rather than per container.
//
// Defining it is never required: a key with no __hash() is hashed structurally,
// field by field. This is the escape hatch, not the mechanism.
pub alias Hashable = impl { fn __hash() u32 }
