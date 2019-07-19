#ifndef UTIL_STRING_PIECE_HASH_H
#define UTIL_STRING_PIECE_HASH_H
#define FUNCTIONAL_HASH_ROTL32(x, r) (x << r) | (x >> (32 - r))

#include "util/string_piece.hh"
#include <functional>


template <typename SizeT>
inline void hash_combine_impl(SizeT& seed, SizeT value) {
    seed ^= value + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

inline void hash_combine_impl(uint32_t& h1, uint32_t k1) {
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    k1 *= c1;
    k1 = FUNCTIONAL_HASH_ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = FUNCTIONAL_HASH_ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
}

inline void hash_combine_impl(uint64_t& h, uint64_t k) {
    const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
    const int r = 47;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;

    // Completely arbitrary number, to prevent 0's
    // from hashing to 0.
    h += 0xe6546b64;
}

template <class T>
inline void hash_combine(std::size_t& seed, T const& v)
{
    hash<T> hasher;
    return hash_combine_impl(seed, hasher(v));
}

template <class It>
inline size_t hash_range(It first, It last) {
    size_t seed = 0;

    for(; first != last; ++first) {
        hash_combine(seed, *first);
    }

    return seed;
}


inline size_t hash_value(const StringPiece &str) {
  return hash_range(str.data(), str.data() + str.length());
}

/* Support for lookup of StringPiece in std::unordered_map<std::string> */
struct StringPieceCompatibleHash : public std::unary_function<const StringPiece &, size_t> {
  size_t operator()(const StringPiece &str) const {
    return hash_value(str);
  }
};

struct StringPieceCompatibleEquals : public std::binary_function<const StringPiece &, const std::string &, bool> {
  bool operator()(const StringPiece &first, const StringPiece &second) const {
    return first == second;
  }
};

template <class T> typename T::const_iterator FindStringPiece(const T &t, const StringPiece &key) {
  return t.find(key, StringPieceCompatibleHash(), StringPieceCompatibleEquals());
}

template <class T> typename T::iterator FindStringPiece(T &t, const StringPiece &key) {
  return t.find(key, StringPieceCompatibleHash(), StringPieceCompatibleEquals());
}

#endif // UTIL_STRING_PIECE_HASH_H
