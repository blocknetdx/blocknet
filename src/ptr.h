#ifndef PTR_H
#define PTR_H

/**
 * Get begin pointer of vector (non-const version).
 * @note These functions avoid the undefined case of indexing into an empty
 * vector, as well as that of indexing after the end of the vector.
 */
template <typename V>
inline typename V::value_type* begin_ptr(V& v)
{
    return v.empty() ? NULL : &v[0];
}
/** Get begin pointer of vector (const version) */
template <typename V>
inline const typename V::value_type* begin_ptr(const V& v)
{
    return v.empty() ? NULL : &v[0];
}
/** Get end pointer of vector (non-const version) */
template <typename V>
inline typename V::value_type* end_ptr(V& v)
{
    return v.empty() ? NULL : (&v[0] + v.size());
}
/** Get end pointer of vector (const version) */
template <typename V>
inline const typename V::value_type* end_ptr(const V& v)
{
    return v.empty() ? NULL : (&v[0] + v.size());
}

#endif // PTR_H
