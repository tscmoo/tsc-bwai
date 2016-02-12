#ifndef TSC_BIT_H
#define TSC_BIT_H

namespace tsc {
;

#ifdef _MSC_VER

template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned long),int>::type = 0>
size_t bit_count_trailing_zeros(T v) {
	unsigned long index;
	_BitScanForward(&index,v);
	return index;
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned int),int>::type = 0>
size_t bit_count_leading_zeros(T v) {
	return __lzcnt(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned int),int>::type = 0>
size_t bit_popcount(T v) {
	return __popcnt(v);
}


#else

template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned int),int>::type = 0>
size_t bit_count_trailing_zeros(T v) {
	return __builtin_ctz(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned long) && sizeof(unsigned long)!=sizeof(unsigned int),int>::type = 0>
size_t bit_count_trailing_zeros(T v) {
	return __builtin_ctzl(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned int),int>::type = 0>
size_t bit_count_leading_zeros(T v) {
	return __builtin_clz(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned long) && sizeof(unsigned long)!=sizeof(unsigned int),int>::type = 0>
size_t bit_count_leading_zeros(T v) {
	return __builtin_clzl(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned int),int>::type = 0>
size_t bit_popcount(T v) {
	return __builtin_popcount(v);
}
template<typename T,typename std::enable_if<sizeof(T)==sizeof(unsigned long) && sizeof(unsigned long)!=sizeof(unsigned int),int>::type = 0>
size_t bit_popcount(T v) {
	return __builtin_popcountl(v);
}

#endif

}

#endif
