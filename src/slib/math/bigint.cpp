#include "../../../inc/slib/math/bigint.h"
#include "../../../inc/slib/core/math.h"
#include "../../../inc/slib/core/io.h"
#include "../../../inc/slib/core/scoped_pointer.h"

#define STACK_BUFFER_SIZE 4096

/*
	CBigInt
*/

#define CBIGINT_INT32(o, v) \
	CBigInt o; \
	sl_uint32 __m__##o; \
	if (v < 0) { \
		__m__##o = -v; \
		o.setSign(-1); \
	} else { \
		__m__##o = v; \
		o.setSign(1); \
	} \
	o.setUserDataElements((sl_uint32*)&__m__##o, 1);

#define CBIGINT_UINT32(o, v) \
	CBigInt o; \
	o.setSign(1); \
	sl_uint32 __m__##o = v; \
	o.setUserDataElements((sl_uint32*)&__m__##o, 1);

#define CBIGINT_INT64(o, v) \
	CBigInt o; \
	if (v < 0) { \
		o.setSign(-1); \
		v = -v; \
	} else { \
		o.setSign(1); \
	} \
	sl_uint32 __m__##o[2]; \
	__m__##o[0] = (sl_uint32)((sl_uint64)v); \
	__m__##o[1] = (sl_uint32)(((sl_uint64)v) >> 32); \
	o.setUserDataElements(__m__##o, 2);

#define CBIGINT_UINT64(o, v) \
	CBigInt o; \
	o.setSign(1); \
	sl_uint32 __m__##o[2]; \
	__m__##o[0] = (sl_uint32)((sl_uint64)v); \
	__m__##o[1] = (sl_uint32)(((sl_uint64)v) >> 32); \
	o.setUserDataElements(__m__##o, 2);

SLIB_MATH_NAMESPACE_BEGIN

SLIB_INLINE static sl_int32 _cbigint_compare(const sl_uint32* a, const sl_uint32* b, sl_uint32 n)
{
	for (sl_uint32 i = n; i > 0; i--) {
		if (a[i - 1] > b[i - 1]) {
			return 1;
		}
		if (a[i - 1] < b[i - 1]) {
			return -1;
		}
	}
	return 0;
}

// returns 0, 1 (overflow)
SLIB_INLINE static sl_uint32 _cbigint_add(sl_uint32* c, const sl_uint32* a, const sl_uint32* b, sl_uint32 n, sl_uint32 _of)
{
	sl_uint32 of = _of;
	for (sl_uint32 i = 0; i < n; i++) {
		sl_uint32 sum = a[i] + of;
		of = sum < of ? 1 : 0;
		sl_uint32 t = b[i];
		sum += t;
		of += sum < t ? 1 : 0;
		c[i] = sum;
	}
	return of;
}

// returns 0, 1 (overflow)
SLIB_INLINE static sl_uint32 _cbigint_add_uint32(sl_uint32* c, const sl_uint32* a, sl_uint32 n, sl_uint32 b)
{
	sl_uint32 of = b;
	if (c == a) {
		for (sl_uint32 i = 0; i < n && of; i++) {
			sl_uint32 sum = a[i] + of;
			of = sum < of ? 1 : 0;
			c[i] = sum;
		}
	} else {
		for (sl_uint32 i = 0; i < n; i++) {
			sl_uint32 sum = a[i] + of;
			of = sum < of ? 1 : 0;
			c[i] = sum;
		}
	}
	return of;
}

// returns 0, 1 (overflow)
SLIB_INLINE static sl_uint32 _cbigint_sub(sl_uint32* c, const sl_uint32* a, const sl_uint32* b, sl_uint32 n, sl_uint32 _of)
{
	sl_uint32 of = _of;
	for (sl_uint32 i = 0; i < n; i++) {
		sl_uint32 k1 = a[i];
		sl_uint32 k2 = b[i];
		sl_uint32 o = k1 < of ? 1 : 0;
		k1 -= of;
		of = o + (k1 < k2 ? 1 : 0);
		k1 -= k2;
		c[i] = k1;
	}
	return of;
}

// returns 0, 1 (overflow)
SLIB_INLINE static sl_uint32 _cbigint_sub_uint32(sl_uint32* c, const sl_uint32* a, sl_uint32 n, sl_uint32 b)
{
	sl_uint32 of = b;
	if (c == a) {
		for (sl_uint32 i = 0; i < n && of; i++) {
			sl_uint32 k = a[i];
			sl_uint32 o = k < of ? 1 : 0;
			k -= of;
			of = o;
			c[i] = k;
		}
	} else {
		for (sl_uint32 i = 0; i < n; i++) {
			sl_uint32 k = a[i];
			sl_uint32 o = k < of ? 1 : 0;
			k -= of;
			of = o;
			c[i] = k;
		}
	}
	return of;
}

// returns overflow
SLIB_INLINE static sl_uint32 _cbigint_mul_uint32(sl_uint32* c, const sl_uint32* a, sl_uint32 n, sl_uint32 b, sl_uint32 o)
{
	sl_uint32 of = o;
	for (sl_uint32 i = 0; i < n; i++) {
		sl_uint64 k = a[i];
		k *= b;
		k += of;
		c[i] = (sl_uint32)k;
		of = (sl_uint32)(k >> 32);
	}
	return of;
}


// c = c + a * b
SLIB_INLINE static sl_uint32 _cbigint_muladd_uint32(sl_uint32* c, const sl_uint32* s, sl_uint32 m, const sl_uint32* a, sl_uint32 n, sl_uint32 b, sl_uint32 o)
{
	n = Math::min(m, n);
	sl_uint32 of = o;
	sl_uint32 i;
	for (i = 0; i < n; i++) {
		sl_uint64 k = a[i];
		k *= b;
		k += of;
		k += s[i];
		c[i] = (sl_uint32)k;
		of = (sl_uint32)(k >> 32);
	}
	if (c == s) {
		for (i = n; i < m && of; i++) {
			sl_uint32 sum = s[i] + of;
			of = sum < of ? 1 : 0;
			c[i] = sum;
		}
	} else {
		for (i = n; i < m; i++) {
			sl_uint32 sum = s[i] + of;
			of = sum < of ? 1 : 0;
			c[i] = sum;
		}
	}
	return of;
}


// returns remainder
SLIB_INLINE static sl_uint32 _cbigint_div_uint32(sl_uint32* q, const sl_uint32* a, sl_uint32 n, sl_uint32 b, sl_uint32 o)
{
	sl_uint32 j = n - 1;
	if (q) {
		for (sl_uint32 i = 0; i < n; i++) {
			sl_uint64 k = o;
			k <<= 32;
			k |= a[j];
			q[j] = (sl_uint32)(k / b);
			o = (sl_uint32)(k % b);
			j--;
		}
	} else {
		for (sl_uint32 i = 0; i < n; i++) {
			sl_uint64 k = o;
			k <<= 32;
			k |= a[j];
			o = (sl_uint32)(k % b);
			j--;
		}
	}
	return o;
}

// shift 0~31 bits
// returns overflow
SLIB_INLINE static sl_uint32 _cbigint_shiftLeft(sl_uint32* c, const sl_uint32* a, sl_uint32 n, sl_uint32 shift, sl_uint32 valueRight)
{
	sl_uint32 rs = 32 - shift;
	sl_uint32 of = valueRight >> rs;
	for (sl_uint32 i = 0; i < n; i++) {
		sl_uint32 t = a[i];
		c[i] = (t << shift) | of;
		of = t >> rs;
	}
	return of;
}

// shift 0~31 bits
// returns overflow
SLIB_INLINE static sl_uint32 _cbigint_shiftRight(sl_uint32* c, const sl_uint32* a, sl_uint32 n, sl_uint32 shift, sl_uint32 valueLeft)
{
	sl_uint32 rs = 32 - shift;
	sl_uint32 of = valueLeft << rs;
	for (sl_uint32 i = n; i > 0; i--) {
		sl_uint32 t = a[i - 1];
		c[i - 1] = (t >> shift) | of;
		of = t << rs;
	}
	return of;
}

SLIB_INLINE static sl_uint32 _cbigint_mse(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = n; ni > 0; ni--) {
		if (a[ni - 1] != 0) {
			return ni;
		}
	}
	return 0;
}

SLIB_INLINE static sl_uint32 _cbigint_lse(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = 0; ni < n; ni++) {
		if (a[ni] != 0) {
			return ni + 1;
		}
	}
	return 0;
}

SLIB_INLINE static sl_uint32 _cbigint_msbytes(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = n; ni > 0; ni--) {
		sl_uint32 e = a[ni - 1];
		if (e != 0) {
			for (sl_uint32 nB = 4; nB > 0; nB--) {
				if (((e >> ((nB - 1) << 3)) & 255) != 0) {
					return (((ni - 1) << 2) + nB);
				}
			}
			break;
		}
	}
	return 0;
}

SLIB_INLINE static sl_uint32 _cbigint_lsbytes(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = 0; ni < n; ni++) {
		sl_uint32 e = a[ni];
		if (e != 0) {
			for (sl_uint32 nB = 0; nB < 4; nB++) {
				if (((e >> (nB << 3)) & 255) != 0) {
					return ((ni << 2) + nB + 1);
				}
			}
			break;
		}
	}
	return 0;
}

SLIB_INLINE static sl_uint32 _cbigint_msbits(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = n; ni > 0; ni--) {
		sl_uint32 e = a[ni - 1];
		if (e != 0) {
			for (sl_uint32 nb = 32; nb > 0; nb--) {
				if (((e >> (nb - 1)) & 1) != 0) {
					return (((ni - 1) << 5) + nb);
				}
			}
			break;
		}
	}
	return 0;
}

SLIB_INLINE static sl_uint32 _cbigint_lsbits(const sl_uint32* a, sl_uint32 n)
{
	for (sl_uint32 ni = 0; ni < n; ni++) {
		sl_uint32 e = a[ni];
		if (e != 0) {
			for (sl_uint32 nb = 0; nb < 32; nb++) {
				if (((e >> nb) & 1) != 0) {
					return ((ni << 5) + nb + 1);
				}
			}
			break;
		}
	}
	return 0;
}

CBigInt::CBigInt()
{
	sign = 1;
	length = 0;
	data = sl_null;
	m_flagUserData = sl_false;
}

CBigInt::~CBigInt()
{
	_free();
}


void CBigInt::_free()
{
	if (data) {
		if (!m_flagUserData) {
			delete[] data;
		}
		data = sl_null;
	}
	sign = 1;
	length = 0;
}

sl_bool CBigInt::getBit(sl_uint32 pos) const
{
	if (pos < (length << 5)) {
		return ((data[pos >> 5] >> (pos & 0x1F)) & 1) != 0;
	} else {
		return sl_false;
	}
}

void CBigInt::setBit(sl_uint32 pos, sl_bool bit)
{
	if (growLength((pos + 31) >> 5)) {
		sl_uint32 ni = pos >> 5;
		sl_uint32 nb = pos & 0x1F;
		if (bit) {
			data[ni] |= (((sl_uint32)(1)) << nb);
		} else {
			data[ni] &= ~(((sl_uint32)(1)) << nb);
		}
	}
}

sl_uint32 CBigInt::getMostSignificantElements() const
{
	if (data) {
		return _cbigint_mse(data, length);
	}
	return 0;
}

sl_uint32 CBigInt::getLeastSignificantElements() const
{
	if (data) {
		return _cbigint_lse(data, length);
	}
	return 0;
}

sl_uint32 CBigInt::getMostSignificantBytes() const
{
	if (data) {
		return _cbigint_msbytes(data, length);
	}
	return 0;
}

sl_uint32 CBigInt::getLeastSignificantBytes() const
{
	if (data) {
		return _cbigint_lsbytes(data, length);
	}
	return 0;
}

sl_uint32 CBigInt::getMostSignificantBits() const
{
	if (data) {
		return _cbigint_msbits(data, length);
	}
	return 0;
}

sl_uint32 CBigInt::getLeastSignificantBits() const
{
	if (data) {
		return _cbigint_lsbits(data, length);
	}
	return 0;
}

void CBigInt::setZero()
{
	if (data) {
		Base::zeroMemory(data, length * 4);
	}
}


CBigInt* CBigInt::allocate(sl_uint32 length)
{
	CBigInt* newObject = new CBigInt;
	if (newObject) {
		if (length > 0) {
			sl_uint32* data = new sl_uint32[length];
			if (data) {
				newObject->m_flagUserData = sl_false;
				newObject->length = length;
				newObject->data = data;
				Base::zeroMemory(data, length * 4);
				return newObject;
			}
			delete newObject;
		} else {
			return newObject;
		}
	}
	return sl_null;
}

CBigInt* CBigInt::duplicate(sl_uint32 newLength) const
{
	CBigInt* ret = allocate(length);
	if (ret) {
		sl_uint32 n = Math::min(length, newLength);
		if (n > 0) {
			Base::copyMemory(ret->data, data, n * 4);
		}
		ret->sign = sign;
		ret->length = newLength;
		return ret;
	}
	return sl_null;
}

sl_bool CBigInt::copyAbsFrom(const CBigInt& other)
{
	if (this == &other) {
		return sl_true;
	}
	sl_uint32 n = other.getSizeInElements();
	if (growLength(n)) {
		if (other.data) {
			Base::copyMemory(data, other.data, n * 4);
			Base::zeroMemory(data + n, (length - n) * 4);
		} else {
			setZero();
		}
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::growLength(sl_uint32 newLength)
{
	if (length >= newLength) {
		return sl_true;
	}
	sl_uint32* newData = new sl_uint32[newLength];
	if (newData) {
		if (data) {
			Base::copyMemory(newData, data, length * 4);
			Base::zeroMemory(newData + length, (newLength - length) * 4);
			if (!m_flagUserData) {
				delete[] data;
			}
		} else {
			Base::zeroMemory(newData, newLength * 4);
		}
		m_flagUserData = sl_false;
		length = newLength;
		data = newData;
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::setLength(sl_uint32 newLength)
{
	if (length < newLength) {
		return growLength(newLength);
	} else if (length == newLength) {
		return sl_true;
	} else {
		if (newLength) {
			sl_uint32* newData = new sl_uint32[newLength];
			if (newData) {
				if (data) {
					Base::copyMemory(newData, data, newLength * 4);
					if (!m_flagUserData) {
						delete[] data;
					}
				}
				m_flagUserData = sl_false;
				length = newLength;
				data = newData;
				return sl_true;
			} else {
				return sl_false;
			}
		} else {
			if (data) {
				if (!m_flagUserData) {
					delete[] data;
				}
			}
			length = 0;
			data = sl_null;
			return sl_true;
		}
	}
}

sl_bool CBigInt::setValueFromElements(const sl_uint32* _data, sl_uint32 n)
{
	sl_uint32 nd = getSizeInElements();
	if (growLength(n)) {
		sl_uint32 i;
		for (i = 0; i < n; i++) {
			data[i] = _data[i];
		}
		for (; i < nd; i++) {
			data[i] = 0;
		}
		return sl_true;
	}
	return sl_false;
}

sl_bool CBigInt::setBytesLE(const void* _bytes, sl_uint32 nBytes)
{
	sl_uint8* bytes = (sl_uint8*)_bytes;
	// remove zeros
	{
		sl_uint32 n;
		for (n = nBytes; n > 0; n--) {
			if (bytes[n - 1]) {
				break;
			}
		}
		nBytes = n;
	}
	setZero();
	if (nBytes) {
		if (growLength((nBytes + 3) >> 2)) {
			for (sl_uint32 i = 0; i < nBytes; i++) {
				data[i >> 2] |= ((sl_uint32)(bytes[i])) << ((i & 3) << 3);
			}
			return sl_true;
		}
	}
	return sl_false;
}

CBigInt* CBigInt::fromBytesLE(const void* bytes, sl_uint32 nBytes)
{
	CBigInt* ret = CBigInt::allocate((nBytes + 3) >> 2);
	if (ret) {
		if (ret->setBytesLE(bytes, nBytes)) {
			return ret;
		}
		delete ret;
	}
	return ret;
}

sl_bool CBigInt::getBytesLE(void* _bytes, sl_uint32 n) const
{
	sl_uint32 size = getSizeInBytes();
	if (n < size) {
		return sl_false;
	}
	sl_uint8* bytes = (sl_uint8*)_bytes;
	sl_uint32 i;
	for (i = 0; i < size; i++) {
		bytes[i] = (sl_uint8)(data[i >> 2] >> ((i & 3) << 3));
	}
	for (; i < n; i++) {
		bytes[i] = 0;
	}
	return sl_true;
}

Memory CBigInt::getBytesLE() const
{
	sl_uint32 size = getSizeInBytes();
	Memory mem = Memory::create(size);
	if (mem.isNotEmpty()) {
		sl_uint8* bytes = (sl_uint8*)(mem.getBuf());
		for (sl_uint32 i = 0; i < size; i++) {
			bytes[i] = (sl_uint8)(data[i >> 2] >> ((i & 3) << 3));
		}
	}
	return mem;
}

sl_bool CBigInt::setBytesBE(const void* _bytes, sl_uint32 nBytes)
{
	sl_uint8* bytes = (sl_uint8*)_bytes;
	// remove zeros
	{
		sl_uint32 n;
		for (n = 0; n < nBytes; n++) {
			if (bytes[n]) {
				break;
			}
		}
		nBytes -= n;
		bytes += n;
	}
	setZero();
	if (nBytes) {
		if (growLength((nBytes + 3) >> 2)) {
			sl_uint32 m = nBytes - 1;
			for (sl_uint32 i = 0; i < nBytes; i++) {
				data[i >> 2] |= ((sl_uint32)(bytes[m])) << ((i & 3) << 3);
				m--;
			}
			return sl_true;
		}
	}
	return sl_false;
}

CBigInt* CBigInt::fromBytesBE(const void* bytes, sl_uint32 nBytes)
{
	CBigInt* ret = CBigInt::allocate((nBytes + 3) >> 2);
	if (ret) {
		if (ret->setBytesBE(bytes, nBytes)) {
			return ret;
		}
		delete ret;
	}
	return ret;
}

sl_bool CBigInt::getBytesBE(void* _bytes, sl_uint32 n) const
{
	sl_uint32 size = getSizeInBytes();
	if (n < size) {
		return sl_false;
	}
	sl_uint8* bytes = (sl_uint8*)_bytes;
	sl_uint32 i;
	sl_uint32 m = n - 1;
	for (i = 0; i < size; i++) {
		bytes[m] = (sl_uint8)(data[i >> 2] >> ((i & 3) << 3));
		m--;
	}
	for (; i < n; i++) {
		bytes[m] = 0;
		m--;
	}
	return sl_true;
}

Memory CBigInt::getBytesBE() const
{
	sl_uint32 size = getSizeInBytes();
	Memory mem = Memory::create(size);
	if (mem.isNotEmpty()) {
		sl_uint8* bytes = (sl_uint8*)(mem.getBuf());
		sl_uint32 m = size - 1;
		for (sl_uint32 i = 0; i < size; i++) {
			bytes[m] = (sl_uint8)(data[i >> 2] >> ((i & 3) << 3));
			m--;
		}
	}
	return mem;
}

sl_bool CBigInt::setValue(sl_int32 v)
{
	if (growLength(1)) {
		if (v < 0) {
			data[0] = -v;
			sign = -1;
		} else {
			data[0] = v;
			sign = 1;
		}
		Base::zeroMemory(data + 1, (length - 1) * 4);
		return sl_true;
	} else {
		return sl_false;
	}
}

CBigInt* CBigInt::fromInt32(sl_int32 v)
{
	CBigInt* ret = allocate(1);
	if (ret) {
		ret->setValue(v);
		return ret;
	}
	return sl_null;
}

sl_bool CBigInt::setValue(sl_uint32 v)
{
	if (growLength(1)) {
		sign = 1;
		data[0] = v;
		Base::zeroMemory(data + 1, (length - 1) * 4);
		return sl_true;
	} else {
		return sl_false;
	}
}

CBigInt* CBigInt::fromUint32(sl_uint32 v)
{
	CBigInt* ret = allocate(1);
	if (ret) {
		ret->setValue(v);
		return ret;
	}
	return sl_null;
}

sl_bool CBigInt::setValue(sl_int64 v)
{
	if (growLength(2)) {
		sl_uint64 _v;
		if (v < 0) {
			_v = v;
			sign = -1;
		} else {
			_v = v;
			sign = 1;
		}
		data[0] = (sl_uint32)(_v);
		data[1] = (sl_uint32)(_v >> 32);
		Base::zeroMemory(data + 2, (length - 2) * 4);
		return sl_true;
	} else {
		return sl_false;
	}
}

CBigInt* CBigInt::fromInt64(sl_int64 v)
{
	CBigInt* ret = allocate(2);
	if (ret) {
		ret->setValue(v);
		return ret;
	}
	return sl_null;
}

sl_bool CBigInt::setValue(sl_uint64 v)
{
	if (growLength(2)) {
		sign = 1;
		data[0] = (sl_uint32)(v);
		data[1] = (sl_uint32)(v >> 32);
		Base::zeroMemory(data + 2, (length - 2) * 4);
		return sl_true;
	} else {
		return sl_false;
	}
}

CBigInt* CBigInt::fromUint64(sl_uint64 v)
{
	CBigInt* ret = allocate(2);
	if (ret) {
		ret->setValue(v);
		return ret;
	}
	return sl_null;
}

template <class CT>
sl_int32 _CBigInt_parseString(CBigInt* out, const CT* sz, sl_uint32 posBegin, sl_uint32 len, sl_uint32 radix)
{
	if (radix < 2 || radix > 64) {
		return SLIB_PARSE_ERROR;;
	}
	sl_int32 sign;
	sl_uint32 pos = posBegin;
	if (pos < len && sz[pos] == '-') {
		pos++;
		sign = -1;
	} else {
		sign = 1;
	}
	for (; pos < len; pos++) {
		sl_int32 c = (sl_uint32)(sz[pos]);
		if (c != '\t' && c != ' ') {
			break;
		}
	}
	sl_uint32 end = pos;
	const sl_uint8* pattern = radix <= 36 ? _StringConv_radixInversePatternSmall : _StringConv_radixInversePatternBig;
	for (; end < len; end++) {
		sl_uint32 c = (sl_uint8)(sz[end]);
		sl_uint32 v = c < 128 ? pattern[c] : 255;
		if (v >= radix) {
			break;
		}
	}
	if (end <= pos) {
		return SLIB_PARSE_ERROR;
	}
	if (!out) {
		return end;
	}
	out->sign = sign;
	if (radix == 16) {
		out->setZero();
		sl_uint32 nh = end - pos;
		sl_uint32 ne = ((nh << 2) + 31) >> 5;
		if (!(out->growLength(ne))) {
			return SLIB_PARSE_ERROR;
		}
		sl_uint32* data = out->data;
		sl_uint32 ih = nh - 1;
		for (; pos < end; pos++) {
			sl_uint32 c = (sl_uint8)(sz[pos]);
			sl_uint32 v = c < 128 ? pattern[c] : 255;
			if (v >= radix) {
				break;
			}
			sl_uint32 ie = ih >> 3;
			sl_uint32 ib = (ih << 2) & 31;
			data[ie] |= (v << ib);
			ih--;
		}
		return pos;
	} else {
		sl_uint32 nb = (sl_uint32)(Math::ceil(Math::log2((double)radix) * len));
		sl_uint32 ne = (nb + 31) >> 5;
		SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, a, ne);
		if (!a) {
			return SLIB_PARSE_ERROR;
		}
		sl_uint32 n = 0;
		for (; pos < end; pos++) {
			sl_uint32 c = (sl_uint8)(sz[pos]);
			sl_uint32 v = c < 128 ? pattern[c] : 255;
			if (v >= radix) {
				break;
			}
			sl_uint32 o = _cbigint_mul_uint32(a, a, n, radix, v);
			if (o) {
				a[n] = o;
				n++;
			}
		}
		if (!(out->setValueFromElements(a, n))) {
			return SLIB_PARSE_ERROR;
		}
		return pos;
	}
}

sl_int32 CBigInt::parseString(CBigInt* out, const char* sz, sl_uint32 posBegin, sl_uint32 len, sl_uint32 radix)
{
	return _CBigInt_parseString(out, sz, posBegin, len, radix);
}

sl_int32 CBigInt::parseString(CBigInt* out, const sl_char16* sz, sl_uint32 posBegin, sl_uint32 len, sl_uint32 radix)
{
	return _CBigInt_parseString(out, sz, posBegin, len, radix);
}

sl_bool CBigInt::parseString(const String& s, sl_uint32 radix)
{
	sl_uint32 n = s.getLength();
	if (n == 0) {
		return sl_false;
	}
	return _CBigInt_parseString(this, s.getBuf(), 0, n, radix) == n;
}

CBigInt* CBigInt::fromString(const String& s, sl_uint32 radix)
{
	CBigInt* ret = new CBigInt;
	if (ret) {
		ret->parseString(s, radix);
	}
	return ret;
}

String CBigInt::toString(sl_uint32 radix) const
{
	if (radix < 2 || radix > 64) {
		return String::null();
	}
	sl_uint32 nb = getSizeInBits();
	if (nb == 0) {
		SLIB_STATIC_STRING(s, "0");
		return s;
	}
	if (radix == 16) {
		sl_uint32 nh = (nb + 3) >> 2;
		sl_uint32 ns;
		if (sign < 0) {
			ns = nh + 1;
		} else {
			ns = nh;
		}
		String ret = String8::allocate(ns);
		if (ret.isNotNull()) {
			sl_char8* buf = ret.getBuf();
			if (sign < 0) {
				buf[0] = '-';
				buf++;
			}
			sl_uint32 ih = nh - 1;
			for (sl_uint32 i = 0; i < nh; i++) {
				sl_uint32 ie = ih >> 3;
				sl_uint32 ib = (ih << 2) & 31;
				sl_uint32 vh = (data[ie] >> ib) & 15;
				if (vh < 10) {
					buf[i] = (sl_char8)(vh + 0x30);
				} else {
					buf[i] = (sl_char8)(vh + 0x37);
				}
				ih--;
			}
		}
		return ret;
	} else {
		sl_uint32 ne = (nb + 31) >> 5;
		sl_uint32 n = (sl_uint32)(Math::ceil((nb + 1) / Math::log2((double)radix))) + 1;
		SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, a, ne);
		if (!a) {
			return String::null();
		}
		SLIB_SCOPED_BUFFER(sl_char8, STACK_BUFFER_SIZE, s, n + 2);
		if (!s) {
			return String::null();
		}
		s = s + n;
		s[1] = 0;
		Base::copyMemory(a, data, ne * 4);
		sl_uint32 l = 0;
		for (; ne > 0;) {
			sl_uint32 v = _cbigint_div_uint32(a, a, ne, radix, 0);
			ne = _cbigint_mse(a, ne);
			if (v < radix) {
				*s = _StringConv_radixPatternUpper[v];
			} else {
				*s = '?';
			}
			s--;
			l++;
		}
		if (sign < 0) {
			*s = '-';
			s--;
			l++;
		}
		return String(s + 1, l);
	}
}


sl_int32 CBigInt::compareAbs(const CBigInt& other) const
{
	const CBigInt& a = *this;
	const CBigInt& b = other;
	sl_uint32 na = a.getSizeInElements();
	sl_uint32 nb = b.getSizeInElements();
	if (na > nb) {
		return 1;
	} else if (na < nb) {
		return -1;
	}
	return _cbigint_compare(a.data, b.data, na);
}

sl_int32 CBigInt::compare(const CBigInt& other) const
{
	const CBigInt& a = *this;
	const CBigInt& b = other;
	sl_uint32 na = a.getSizeInElements();
	sl_uint32 nb = b.getSizeInElements();
	if (na == 0) {
		if (nb == 0) {
			return 0;
		} else {
			return -b.sign;
		}
	} else {
		if (nb == 0) {
			return a.sign;
		}
	}
	if (a.sign >= 0 && b.sign < 0) {
		return 1;
	} else if (a.sign < 0 && b.sign >= 0) {
		return -1;
	}
	if (na > nb) {
		return a.sign;
	} else if (na < nb) {
		return -a.sign;
	}
	return _cbigint_compare(a.data, b.data, na) * a.sign;
}

sl_int32 CBigInt::compare(sl_int32 v) const
{
	CBIGINT_INT32(o, v);
	return compare(o);
}

sl_int32 CBigInt::compare(sl_uint32 v) const
{
	CBIGINT_UINT32(o, v);
	return compare(o);
}

sl_int32 CBigInt::compare(sl_int64 v) const
{
	CBIGINT_INT64(o, v);
	return compare(o);
}

sl_int32 CBigInt::compare(sl_uint64 v) const
{
	CBIGINT_UINT64(o, v);
	return compare(o);
}

sl_bool CBigInt::addAbs(const CBigInt& a, const CBigInt& b)
{
	sl_uint32 na = a.getSizeInElements();
	sl_uint32 nb = b.getSizeInElements();
	if (na == 0) {
		if (nb) {
			if (this != &b) {
				return copyAbsFrom(b);
			} else {
				return sl_true;
			}
		}
		return sl_true;
	}
	if (nb == 0) {
		if (this != &a) {
			return copyAbsFrom(a);
		}
		return sl_true;
	}
	sl_uint32 nd;
	if (&a == this) {
		nd = na;
	} else if (&b == this) {
		nd = nb;
	} else {
		nd = getSizeInElements();
	}
	const CBigInt* _p;
	const CBigInt* _q;
	sl_uint32 np, nq;
	if (na > nb) {
		_p = &b;
		np = nb;
		_q = &a;
		nq = na;
	} else {
		_p = &a;
		np = na;
		_q = &b;
		nq = nb;
	}
	const CBigInt& p = *_p;
	const CBigInt& q = *_q;
	if (growLength(nq)) {
		sl_uint32 of = _cbigint_add(data, q.data, p.data, np, 0);
		if (of) {
			of = _cbigint_add_uint32(data + np, q.data + np, nq - np, of);
			if (of) {
				if (growLength(nq + 1)) {
					data[nq] = of;
					nq++;
				} else {
					return sl_false;
				}
			}
		} else {
			if (data != q.data) {
				Base::copyMemory(data + np, q.data + np, (nq - np) * 4);
			}
		}
		for (sl_uint32 i = nq; i < nd; i++) {
			data[i] = 0;
		}
		return sl_true;
	}
	return sl_false;
}

sl_bool CBigInt::addAbs(const CBigInt& a, sl_uint32 v)
{
	CBIGINT_UINT32(o, v);
	return addAbs(a, o);
}

sl_bool CBigInt::addAbs(const CBigInt& a, sl_uint64 v)
{
	CBIGINT_UINT64(o, v);
	return addAbs(a, o);
}

sl_bool CBigInt::add(const CBigInt& a, const CBigInt& b)
{
	if (a.sign * b.sign < 0) {
		if (a.compareAbs(b) >= 0) {
			sign = a.sign;
			return subAbs(a, b);
		} else {
			sign = - a.sign;
			return subAbs(b, a);
		}
	} else {
		sign = a.sign;
		return addAbs(a, b);
	}
}

sl_bool CBigInt::add(const CBigInt& a, sl_int32 v)
{
	CBIGINT_INT32(o, v);
	return add(a, o);
}

sl_bool CBigInt::add(const CBigInt& a, sl_uint32 v)
{
	CBIGINT_UINT32(o, v);
	return add(a, o);
}

sl_bool CBigInt::add(const CBigInt& a, sl_int64 v)
{
	CBIGINT_INT64(o, v);
	return add(a, o);
}

sl_bool CBigInt::add(const CBigInt& a, sl_uint64 v)
{
	CBIGINT_UINT64(o, v);
	return add(a, o);
}

sl_bool CBigInt::subAbs(const CBigInt& a, const CBigInt& b)
{
	sl_uint32 na = a.getSizeInElements();
	sl_uint32 nb = b.getSizeInElements();
	if (nb == 0) {
		if (this != &a) {
			return copyAbsFrom(a);
		}
		return sl_true;
	}
	if (na < nb) {
		return sl_false;
	}
	sl_uint32 nd;
	if (&a == this) {
		nd = na;
	} else if (&b == this) {
		nd = nb;
		if (!growLength(na)) {
			return sl_false;
		}
	} else {
		nd = getSizeInElements();
		if (!growLength(na)) {
			return sl_false;
		}
	}
	sl_uint32 of = _cbigint_sub(data, a.data, b.data, nb, 0);
	if (of) {
		of = _cbigint_sub_uint32(data + nb, a.data + nb, na - nb, of);
		if (of) {
			return sl_false;
		}
	} else {
		if (data != a.data) {
			Base::copyMemory(data + nb, a.data + nb, (na - nb) * 4);
		}
	}
	for (sl_uint32 i = na; i < nd; i++) {
		data[i] = 0;
	}
	return sl_true;
}

sl_bool CBigInt::subAbs(const CBigInt& a, sl_uint32 v)
{
	CBIGINT_UINT32(o, v);
	return addAbs(a, o);
}

sl_bool CBigInt::subAbs(const CBigInt& a, sl_uint64 v)
{
	CBIGINT_UINT64(o, v);
	return addAbs(a, o);
}

sl_bool CBigInt::sub(const CBigInt& a, const CBigInt& b)
{
	if (a.sign * b.sign > 0) {
		if (a.compareAbs(b) >= 0) {
			sign = a.sign;
			return subAbs(a, b);
		} else {
			sign = - a.sign;
			return subAbs(b, a);
		}
	} else {
		sign = a.sign;
		return addAbs(a, b);
	}
}

sl_bool CBigInt::sub(const CBigInt& a, sl_int32 v)
{
	CBIGINT_INT32(o, v);
	return sub(a, o);
}

sl_bool CBigInt::sub(const CBigInt& a, sl_uint32 v)
{
	CBIGINT_UINT32(o, v);
	return sub(a, o);
}

sl_bool CBigInt::sub(const CBigInt& a, sl_int64 v)
{
	CBIGINT_INT64(o, v);
	return sub(a, o);
}

sl_bool CBigInt::sub(const CBigInt& a, sl_uint64 v)
{
	CBIGINT_UINT64(o, v);
	return sub(a, o);
}

sl_bool CBigInt::mulAbs(const CBigInt& a, const CBigInt& b)
{
	sl_uint32 na = a.getSizeInElements();
	sl_uint32 nb = b.getSizeInElements();
	if (na == 0 || nb == 0) {
		setZero();
		return sl_true;
	}
	sl_uint32 nd;
	if (&a == this) {
		nd = na;
	} else if (&b == this) {
		nd = nb;
	} else {
		nd = getSizeInElements();
	}
	sl_uint32 n = na + nb;
	SLIB_SCOPED_BUFFER(sl_uint64, STACK_BUFFER_SIZE, out, n);
	if (!out) {
		return sl_false;
	}
	Base::zeroMemory(out, 8 * n);
	for (sl_uint32 ib = 0; ib < nb; ib++) {
		for (sl_uint32 ia = 0; ia < na; ia++) {
			sl_uint64 c = a.data[ia];
			c *= b.data[ib];
			out[ia + ib] += (sl_uint32)c;
			out[ia + ib + 1] += (sl_uint32)(c >> 32);
		}
	}
	sl_uint32 o = 0;
	sl_uint32 i;
	sl_uint32 m = 0;
	for (i = 0; i < n; i++) {
		sl_uint64 c = out[i] + o;
		sl_uint32 t = (sl_uint32)c;
		out[i] = t;
		if (t) {
			m = i;
		}
		o = (sl_uint32)(c >> 32);
	}
	if (growLength(m + 1)) {
		for (i = 0; i <= m; i++) {
			data[i] = (sl_uint32)(out[i]);
		}
		for (; i < nd; i++) {
			data[i] = 0;
		}
		return sl_true;
	}
	return sl_false;
}

sl_bool CBigInt::mul(const CBigInt& a, const CBigInt& b)
{
	sign = a.sign * b.sign;
	return mulAbs(a, b);
}

sl_bool CBigInt::mulAbs(const CBigInt& a, sl_uint32 b)
{
	sl_uint32 na = a.getSizeInElements();
	if (na == 0 || b == 0) {
		setZero();
		return sl_true;
	}
	sl_uint32 nd;
	if (&a == this) {
		nd = na;
	} else {
		nd = getSizeInElements();
	}
	sl_uint32 n = na + 1;
	SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, out, n);
	if (!out) {
		return sl_false;
	}
	sl_uint32 o = _cbigint_mul_uint32(out, a.data, na, b, 0);
	if (o == 0) {
		n = na;
	} else {
		out[n - 1] = o;
	}
	return setValueFromElements(out, n);
}

sl_bool CBigInt::mul(const CBigInt& a, sl_int32 v)
{
	if (v < 0) {
		sign = -a.sign;
		v = -v;
	} else {
		sign = a.sign;
	}
	return mulAbs(a, v);
}

sl_bool CBigInt::mul(const CBigInt& a, sl_uint32 v)
{
	return mulAbs(a, v);
}

sl_bool CBigInt::mul(const CBigInt& a, sl_int64 v)
{
	CBIGINT_INT64(o, v);
	return mul(a, o);
}

sl_bool CBigInt::mul(const CBigInt& a, sl_uint64 v)
{
	CBIGINT_UINT64(o, v);
	return mul(a, o);
}

sl_bool CBigInt::divAbs(const CBigInt& a, const CBigInt& b, CBigInt* quotient, CBigInt* remainder)
{
	sl_uint32 nba = a.getSizeInBits();
	sl_uint32 nbb = b.getSizeInBits();
	if (nbb == 0) {
		return sl_false;
	}
	if (nba == 0) {
		if (remainder) {
			remainder->setZero();
		}
		if (quotient) {
			quotient->setZero();
		}
		return sl_true;
	}
	if (nba < nbb) {
		if (remainder) {
			if (!remainder->copyAbsFrom(a)) {
				return sl_false;
			}
		}
		if (quotient) {
			quotient->setZero();
		}
		return sl_true;
	}
	sl_uint32 na = (nba + 31) >> 5;
	sl_uint32 nb = (nbb + 31) >> 5;
	sl_uint32 nbc = nba - nbb;
	SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, _tmem, (nb + 1) * 31);
	if (!_tmem) {
		return sl_false;
	}
	sl_uint32* tb[32];
	sl_uint32 tl[32];
	{
		sl_uint32 n = Math::min(31u, nbc);
		tb[0] = (sl_uint32*)(b.data);
		tl[0] = nb;
		for (sl_uint32 i = 1; i <= n; i++) {
			tb[i] = _tmem + ((i - 1) * (nb + 1));
			tl[i] = (nbb + i + 31) >> 5;
			sl_uint32 o = _cbigint_shiftLeft(tb[i], b.data, nb, i, 0);
			if (o) {
				tb[i][nb] = o;
			}
		}
	}
	SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, rem, na * 2);
	if (!rem) {
		return sl_false;
	}
	Base::copyMemory(rem, a.data, na * 4);
	sl_uint32* q = rem + na;
	Base::zeroMemory(q, na * 4);
	sl_uint32 nbr = nba;
	sl_uint32 shift = nbc;
	sl_uint32 nq = 0;
	for (sl_uint32 i = 0; i <= nbc; i++) {
		sl_uint32 se = shift >> 5;
		sl_uint32 sb = shift & 31;
		sl_uint32 nbs = nbb + shift;
		if (nbs < nbr || (nbs == nbr && _cbigint_compare(rem + se, tb[sb], tl[sb]) >= 0)) {
			if (_cbigint_sub(rem + se, rem + se, tb[sb], tl[sb], 0)) {
				rem[se + tl[sb]] = 0;
			}
			q[se] |= (1 << sb);
			if (nq == 0) {
				nq = se + 1;
			}
			nbr = _cbigint_msbits(rem, se + tl[sb]);
		}
		shift--;
	}
	if (quotient) {
		if (!quotient->setValueFromElements(q, nq)) {
			return sl_false;
		}
	}
	sl_uint32 nr = (nbr + 31) >> 5;
	if (remainder) {
		if (! remainder->setValueFromElements(rem, nr)) {
			return sl_false;
		}
	}
	return sl_true;
}

sl_bool CBigInt::divAbs(const CBigInt& a, sl_uint32 b, CBigInt* quotient, sl_uint32* remainder)
{
	if (b == 0) {
		return sl_false;
	}
	sl_uint32 na = a.getSizeInElements();
	if (na == 0) {
		if (remainder) {
			*remainder = 0;
		}
		if (quotient) {
			quotient->setZero();
		}
		return sl_true;
	}
	sl_uint32* q;
	if (quotient) {
		quotient->setZero();
		if (quotient->growLength(na)) {
			q = quotient->data;
		} else {
			return sl_false;
		}
	} else {
		q = sl_null;
	}
	sl_uint32 r = _cbigint_div_uint32(q, a.data, na, b, 0);
	if (remainder) {
		*remainder = r;
	}
	return sl_true;
}

sl_bool CBigInt::div(const CBigInt& a, const CBigInt& b, CBigInt* quotient, CBigInt* remainder)
{
	if (divAbs(a, b, quotient, remainder)) {
		if (quotient) {
			if (a.sign < 0) {
				if (quotient->addAbs(*quotient, (sl_uint32)1)) {
					quotient->sign = -b.sign;
				} else {
					return sl_false;
				}
			} else {
				quotient->sign = b.sign;
			}
		}
		if (remainder) {
			if (a.sign < 0) {
				if (remainder->subAbs(b, *remainder)) {
					remainder->sign = 1;
				} else {
					return sl_false;
				}
			} else {
				remainder->sign = 1;
			}
		}
		return sl_true;
	} else {
		return sl_true;
	}
}

sl_bool CBigInt::div(const CBigInt& a, sl_int32 b, CBigInt* quotient, sl_uint32* remainder)
{
	sl_int32 v;
	sl_int32 s;
	if (b > 0) {
		v = b;
		s = 1;
	} else {
		v = -b;
		s = -1;
	}
	if (divAbs(a, (sl_uint32)v, quotient, remainder)) {
		if (quotient) {
			if (a.sign < 0) {
				if (quotient->addAbs(*quotient, (sl_uint32)1)) {
					quotient->sign = -s;
				} else {
					return sl_false;
				}
			} else {
				quotient->sign = s;
			}
		}
		if (remainder) {
			if (a.sign < 0) {
				*remainder = b - *remainder;
			}
		}
		return sl_true;
	} else {
		return sl_true;
	}
}

sl_bool CBigInt::div(const CBigInt& a, sl_uint32 b, CBigInt* quotient, sl_uint32* remainder)
{
	if (divAbs(a, b, quotient, remainder)) {
		if (quotient) {
			if (a.sign < 0) {
				if (quotient->addAbs(*quotient, (sl_uint32)1)) {
					quotient->sign = -1;
				} else {
					return sl_false;
				}
			} else {
				quotient->sign = 1;
			}
		}
		if (remainder) {
			if (a.sign < 0) {
				*remainder = b - *remainder;
			}
		}
		return sl_true;
	} else {
		return sl_true;
	}
}

sl_bool CBigInt::div(const CBigInt& a, sl_int64 b, CBigInt* quotient, sl_uint64* remainder)
{
	CBIGINT_INT64(o, b);
	CBigInt* r;
	if (remainder) {
		r = new CBigInt;
		if (!r) {
			return sl_false;
		}
	} else {
		r = sl_null;
	}
	if (div(a, o, quotient, r)) {
		if (remainder) {
			sl_uint8 bytes[8];
			r->getBytesLE(bytes, 8);
			*remainder = MIO::readUint64(bytes);
		}
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::div(const CBigInt& a, sl_uint64 b, CBigInt* quotient, sl_uint64* remainder)
{
	CBIGINT_UINT64(o, b);
	CBigInt* r;
	if (remainder) {
		r = new CBigInt;
		if (!r) {
			return sl_false;
		}
	} else {
		r = sl_null;
	}
	if (div(a, o, quotient, r)) {
		if (remainder) {
			sl_uint8 bytes[8];
			r->getBytesLE(bytes, 8);
			*remainder = MIO::readUint64(bytes);
		}
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::shiftLeft(const CBigInt& a, sl_uint32 shift)
{
	if (shift == 0) {
		return copyFrom(a);
	}
	sl_uint32 nba = a.getSizeInBits();
	sl_uint32 nd;
	if (&a == this) {
		nd = (nba + 31) >> 5;
	} else {
		sign = a.sign;
		nd = getSizeInElements();
	}
	sl_uint32 nbt = nba + shift;
	sl_uint32 nt = (nbt + 31) >> 5;
	if (growLength(nt)) {
		sl_uint32 se = shift >> 5;
		sl_uint32 sb = shift & 31;
		if (se > 0 || data != a.data) {
			sl_uint32 i;
			for (i = nt; i > se; i--) {
				data[i - 1] = a.data[i - 1 - se];
			}
			for (; i > 0; i--) {
				data[i - 1] = 0;
			}
		}
		if (sb > 0) {
			_cbigint_shiftLeft(data, data, nt, sb, 0);
		}
		for (sl_uint32 i = nt; i < nd; i++) {
			data[i] = 0;
		}
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::shiftRight(const CBigInt& a, sl_uint32 shift)
{
	if (shift == 0) {
		return copyFrom(a);
	}	
	sl_uint32 nba = a.getSizeInBits();
	if (nba <= shift) {
		setZero();
		return sl_true;
	}
	sl_uint32 nd;
	if (&a == this) {
		nd = (nba + 31) >> 5;
	} else {
		sign = a.sign;
		nd = getSizeInElements();
	}
	sl_uint32 nbt = nba - shift;
	sl_uint32 nt = (nbt + 31) >> 5;
	if (growLength(nt)) {
		sl_uint32 se = shift >> 5;
		sl_uint32 sb = shift & 31;
		if (se > 0 || data != a.data) {
			sl_uint32 i;
			for (i = 0; i < nt; i++) {
				data[i] = a.data[i + se];
			}
		}
		if (sb > 0) {
			sl_uint32 l;
			if (nt + se < a.length) {
				l = a.data[nt + se];
			} else {
				l = 0;
			}
			_cbigint_shiftRight(data, data, nt, sb, l);
		}
		for (sl_uint32 i = nt; i < nd; i++) {
			data[i] = 0;
		}
		return sl_true;
	} else {
		return sl_false;
	}
}

sl_bool CBigInt::pow(const CBigInt& A, const CBigInt& E, const CBigInt* pM)
{
	if (pM) {
		sl_uint32 nM = pM->getSizeInElements();
		if (nM == 0) {
			return sl_false;
		}
	}
	sl_uint32 nbE = E.getSizeInBits();
	if (nbE == 0) {
		if (!setValue((sl_uint32)1)) {
			return sl_false;
		}
		sign = 1;
		return sl_true;
	}
	if (E.sign < 0) {
		return sl_false;
	}
	sl_uint32 nA = A.getSizeInElements();
	if (nA == 0) {
		setZero();
		return sl_true;
	}
	CBigInt T;
	if (!T.copyFrom(A)) {
		return sl_false;
	}
	const CBigInt* TE;
	CBigInt _TE;
	if (this == &E) {
		TE = &_TE;
		if (!_TE.copyFrom(E)) {
			return sl_false;
		}
	} else {
		TE = &E;
	}
	if (!setValue((sl_uint32)1)) {
		return sl_false;
	}
	for (sl_uint32 ib = 0; ib < nbE; ib++) {
		sl_uint32 ke = ib >> 5;
		sl_uint32 kb = ib & 31;
		if ((TE->data[ke] >> kb) & 1) {
			if (!mul(*this, T)) {
				return sl_false;
			}
			if (pM) {
				if (!CBigInt::div(*this, *pM, sl_null, this)) {
					return sl_false;
				}
			}
		}
		if (!T.mul(T, T)) {
			return sl_false;
		}
		if (pM) {
			if (!CBigInt::div(T, *pM, sl_null, &T)) {
				return sl_false;
			}
		}
	}
	return sl_true;
}

sl_bool CBigInt::pow(const CBigInt& A, sl_uint32 E, const CBigInt* pM)
{
	CBIGINT_UINT32(o, E);
	return pow(A, o, pM);
}

/*
	Montgomery multiplication: A = A * B * R^-1 mod M
*/
static sl_bool _cbigint_mont_mul(CBigInt& A, const CBigInt& B, const CBigInt& M, sl_uint32 MI)
{
	sl_uint32 nM = M.length;
	sl_uint32 nB = Math::min(nM, B.length);
	
	sl_uint32 nOut = nM * 2 + 1;
	SLIB_SCOPED_BUFFER(sl_uint32, STACK_BUFFER_SIZE, out, nOut);
	if (!out) {
		return sl_false;
	}
	Base::zeroMemory(out, nOut * 4);
	for (sl_uint32 i = 0; i < nM; i++) {
		// T = (T + cB*B + cM*M) / 2^(32*nM)
		sl_uint32 cB = i < A.length ? A.data[i] : 0;
		sl_uint32 cM = (out[0] + cB * B.data[0]) * MI;
		_cbigint_muladd_uint32(out, out, nOut, B.data, nB, cB, 0);
		_cbigint_muladd_uint32(out, out, nOut, M.data, nM, cM, 0);
		*out = cB;
		nOut--;
		out++;
	}
	if (!A.setValueFromElements(out, nM + 1)) {
		return sl_false;
	}
	if (A.compareAbs(M) >= 0) {
		if (!A.subAbs(A, M)) {
			return sl_false;
		}
	}
	return sl_true;
}

/*
	Montgomery reduction: A = A * R^-1 mod M
*/
static sl_bool _cbigint_mont_reduction(CBigInt& A, const CBigInt& M, sl_uint32 MI)
{
	CBIGINT_UINT32(o, 1);
	return _cbigint_mont_mul(A, o, M, MI);
}

sl_bool CBigInt::pow_montgomery(const CBigInt& A, const CBigInt& _E, const CBigInt& _M)
{
	CBigInt M;
	M.copyFrom(_M);
	if (!M.compact()) {
		return sl_false;
	}
	sl_uint32 nM = M.getSizeInElements();
	if (nM == 0) {
		return sl_false;
	}
	if (M.sign < 0) {
		return sl_false;
	}
	const CBigInt* pE;
	CBigInt __E;
	if (&_E == this) {
		pE = &__E;
		if (!__E.copyFrom(_E)) {
			return sl_false;
		}
	} else {
		pE = &_E;
	}
	const CBigInt& E = *pE;
	sl_uint32 nE = E.getSizeInElements();
	if (nE == 0) {
		if (!setValue((sl_uint32)1)) {
			return sl_false;
		}
		sign = 1;
		return sl_true;
	}
	if (E.sign < 0) {
		return sl_false;
	}
	sl_uint32 nA = A.getSizeInElements();
	if (nA == 0) {
		setZero();
		return sl_true;
	}

	// MI = -(M0^-1) mod (2^32)
	sl_uint32 MI;
	// initialize montgomery
	{
		sl_uint32 M0 = M.data[0];
		sl_uint32 K = M0;
		K += ((M0 + 2) & 4) << 1;
		for (sl_uint32 i = 32; i >= 8; i /= 2) {
			K *= (2 - (M0 * K));
		}
		MI = 0 - K;
	}

	// pre-compute R^2 mod M
	// R = 2^(nM*32)
	CBigInt R2;
	{
		if (!R2.setValue((sl_uint32)1)) {
			return sl_false;
		}
		if (!R2.shiftLeft(nM * 64)) {
			return sl_false;
		}
		if (!CBigInt::divAbs(R2, M, sl_null, &R2)) {
			return sl_false;
		}
	}

	sl_bool flagNegative = A.sign < 0;
	// T = A * R^2 * R^-1 mod M = A * R mod M
	CBigInt T;
	if (!CBigInt::divAbs(A, M, sl_null, &T)) {
		return sl_false;
	}
	if (!_cbigint_mont_mul(T, R2, M, MI)) {
		return sl_false;
	}

	// C = R^2 * R^-1 mod M = R mod M
	if (!copyFrom(R2)) {
		return sl_false;
	}
	if (!_cbigint_mont_reduction(*this, M, MI)) {
		return sl_false;
	}

	sl_uint32 nbE = E.getSizeInBits();
	for (sl_uint32 ib = 0; ib < nbE; ib++) {
		sl_uint32 ke = ib >> 5;
		sl_uint32 kb = ib & 31;
		if ((E.data[ke] >> kb) & 1) {
			// C = C * T * R^-1 mod M
			if (!_cbigint_mont_mul(*this, T, M, MI)) {
				return sl_false;
			}
		}
		// T = T * T * R^-1 mod M
		if (!_cbigint_mont_mul(T, T, M, MI)) {
			return sl_false;
		}
	}
	if (!_cbigint_mont_reduction(*this, M, MI)) {
		return sl_false;
	}
	if (flagNegative && (E.data[0] & 1) != 0) {
		sign = -1;
		if (!add(M)) {
			return sl_false;
		}
	} else {
		sign = 1;
	}
	return sl_true;
}

sl_bool CBigInt::inverseMod(const CBigInt& A, const CBigInt& M)
{
	sl_uint32 nM = M.getSizeInElements();
	if (nM == 0) {
		return sl_false;
	}
	if (M.sign < 0) {
		return sl_false;
	}
	sl_uint32 nA = A.getSizeInElements();
	if (nA == 0) {
		return sl_false;
	}
	CBigInt G;
	if (!G.gcd(A, M)) {
		return sl_false;
	}
	if (G.compare((sl_uint32)1) != 0) {
		return sl_false;
	}
	CBigInt Xa;
	if (!CBigInt::div(A, M, sl_null, &Xa)) {
		return sl_false;
	}
	CBigInt Xb;
	if (!Xb.copyFrom(M)) {
		return sl_false;
	}
	CBigInt T1;
	if (!T1.copyFrom(Xa)) {
		return sl_false;
	}
	CBIGINT_INT32(T1a, 1);
	CBIGINT_INT32(T1b, 0);
	CBigInt T2;
	if (!T2.copyFrom(M)) {
		return sl_false;
	}
	CBIGINT_INT32(T2a, 0);
	CBIGINT_INT32(T2b, 1);

	for (;;) {
		while ((T1.data[0] & 1) == 0) {
			if (!T1.shiftRight(1)) {
				return sl_false;
			}
			if ((T1a.data[0] & 1) != 0 || (T1b.data[0] & 1) != 0) {
				if (!T1a.add(Xb)) {
					return sl_false;
				}
				if (!T1b.sub(Xa)) {
					return sl_false;
				}
			}
			if (!T1a.shiftRight(1)) {
				return sl_false;
			}
			if (!T1b.shiftRight(1)) {
				return sl_false;
			}
		}
		while ((T2.data[0] & 1) == 0) {
			if (!T2.shiftRight(1)) {
				return sl_false;
			}
			if ((T2a.data[0] & 1) != 0 || (T2b.data[0] & 1) != 0) {
				if (!T2a.add(Xb)) {
					return sl_false;
				}
				if (!T2b.sub(Xa)) {
					return sl_false;
				}
			}
			if (!T2a.shiftRight(1)) {
				return sl_false;
			}
			if (!T2b.shiftRight(1)) {
				return sl_false;
			}
		}
		if (T1.compare(T2) >= 0) {
			if (!T1.sub(T2)) {
				return sl_false;
			}
			if (!T1a.sub(T2a)) {
				return sl_false;
			}
			if (!T1b.sub(T2b)) {
				return sl_false;
			}
		} else {
			if (!T2.sub(T1)) {
				return sl_false;
			}
			if (!T2a.sub(T1a)) {
				return sl_false;
			}
			if (!T2b.sub(T1b)) {
				return sl_false;
			}
		}
		if (T1.isZero()) {
			break;
		}
	}
	while (T2a.compare((sl_uint32)0) < 0) {
		if (!T2a.add(M)) {
			return sl_false;
		}
	}
	while (T2a.compare(M) >= 0) {
		if (!T2a.sub(M)) {
			return sl_false;
		}
	}
	if (!copyFrom(T2a)) {
		return sl_false;
	}
	sign = A.sign;
	return sl_true;
}

sl_bool CBigInt::gcd(const CBigInt& _A, const CBigInt& _B)
{
	if (&_A == &_B) {
		sign = 1;
		return copyAbsFrom(_A);
	}
	sl_uint32 lbA = _A.getLeastSignificantBits();
	sl_uint32 lbB = _B.getLeastSignificantBits();
	if (lbA == 0 || lbB == 0) {
		setZero();
		return sl_true;
	}
	sl_uint32 min_p2 = Math::min(lbA - 1, lbB - 1);
	CBigInt A, B;
	if (!A.shiftRight(_A, min_p2)) {
		return sl_false;
	}
	if (!B.shiftRight(_B, min_p2)) {
		return sl_false;
	}
	for (;;) {
		lbA = A.getLeastSignificantBits();
		if (lbA == 0) {
			break;
		}
		if (!A.shiftRight(lbA - 1)) {
			return sl_false;
		}
		lbB = B.getLeastSignificantBits();
		if (lbB == 0) {
			break;
		}
		if (!B.shiftRight(lbB - 1)) {
			return sl_false;
		}
		if (A.compareAbs(B) >= 0) {
			if (!A.subAbs(A, B)) {
				return sl_false;
			}
			if (!A.shiftRight(1)) {
				return sl_false;
			}
		} else {
			if (!B.subAbs(B, A)) {
				return sl_false;
			}
			if (!B.shiftRight(1)) {
				return sl_false;
			}
		}
	}
	if (!shiftLeft(B, min_p2)) {
		return sl_false;
	}
	sign = 1;
	return sl_true;
}


/*
	BigInt
*/

String BigInt::toString(sl_uint32 radix) const
{
	CBigInt* o = m_object.get();
	if (o) {
		return o->toString(radix);
	} else {
		SLIB_STATIC_STRING(s, "0");
		return s;
	}
}

sl_int32 BigInt::compare(const BigInt& other) const
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (a) {
		if (b) {
			return a->compare(*b);
		} else {
			return a->getSign();
		}
	} else {
		if (b) {
			return -(b->getSign());
		} else {
			return 0;
		}
	}
}

sl_int32 BigInt::compare(sl_int32 v) const
{
	CBigInt* a = m_object.get();
	if (a) {
		return a->compare(v);
	} else {
		if (v > 0) {
			return -1;
		} else if (v < 0) {
			return 1;
		} else {
			return 0;
		}
	}
}

sl_int32 BigInt::compare(sl_uint32 v) const
{
	CBigInt* a = m_object.get();
	if (a) {
		return a->compare(v);
	} else {
		return -1;
	}
}

sl_int32 BigInt::compare(sl_int64 v) const
{
	CBigInt* a = m_object.get();
	if (a) {
		return a->compare(v);
	} else {
		if (v > 0) {
			return -1;
		} else if (v < 0) {
			return 1;
		} else {
			return 0;
		}
	}
}

sl_int32 BigInt::compare(sl_uint64 v) const
{
	CBigInt* a = m_object.get();
	if (a) {
		return a->compare(v);
	} else {
		return -1;
	}
}

BigInt BigInt::add(const BigInt& A, const BigInt& B)
{
	CBigInt* a = A.m_object.get();
	CBigInt* b = B.m_object.get();
	if (b) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->add(*a, *b)) {
					return r;
				}
				delete r;
			}
		} else {
			return b;
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::add(const BigInt& other)
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (b) {
		if (a) {
			return a->add(*a, *b);
		} else {
			a = b->duplicate();
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::add(const BigInt& A, sl_int32 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->add(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromInt32(v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::add(sl_int32 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->add(*a, v);
		} else {
			a = CBigInt::fromInt32(v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::add(const BigInt& A, sl_uint32 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->add(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromUint32(v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::add(sl_uint32 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->add(*a, v);
		} else {
			a = CBigInt::fromUint32(v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::add(const BigInt& A, sl_int64 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->add(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromInt64(v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::add(sl_int64 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->add(*a, v);
		} else {
			a = CBigInt::fromInt64(v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::add(const BigInt& A, sl_uint64 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->add(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromUint64(v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::add(sl_uint64 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->add(*a, v);
		} else {
			a = CBigInt::fromUint64(v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}


BigInt BigInt::sub(const BigInt& A, const BigInt& B)
{
	CBigInt* a = A.m_object.get();
	CBigInt* b = B.m_object.get();
	if (b) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->sub(*a, *b)) {
					return r;
				}
				delete r;
			}
		} else {
			CBigInt* r = b->duplicate();
			if (r) {
				r->makeNagative();
				return r;
			}
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::sub(const BigInt& other)
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (b) {
		if (a) {
			return a->sub(*a, *b);
		} else {
			a = b->duplicate();
			if (a) {
				a->makeNagative();
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::sub(const BigInt& A, sl_int32 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->sub(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromInt32(-v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::sub(sl_int32 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->sub(*a, v);
		} else {
			a = CBigInt::fromInt32(-v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::sub(const BigInt& A, sl_uint32 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->sub(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromInt64(-((sl_int64)v));
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::sub(sl_uint32 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->sub(*a, v);
		} else {
			a = CBigInt::fromUint32(v);
			if (a) {
				a->makeNagative();
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::sub(const BigInt& A, sl_int64 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->sub(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			return CBigInt::fromInt64(-v);
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::sub(sl_int64 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->sub(*a, v);
		} else {
			a = CBigInt::fromInt64(-v);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::sub(const BigInt& A, sl_uint64 v)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->sub(*a, v)) {
					return r;
				}
				delete r;
			}
		} else {
			CBigInt* r = CBigInt::fromUint64(v);
			if (r) {
				r->makeNagative();
				return r;
			}
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::sub(sl_uint64 v)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			return a->sub(*a, v);
		} else {
			a = CBigInt::fromUint64(v);
			if (a) {
				a->makeNagative();
				m_object = a;
				return sl_true;
			}
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::mul(const BigInt& A, const BigInt& B)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		CBigInt* b = B.m_object.get();
		if (b) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->mul(*a, *b)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::mul(const BigInt& other)
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (a) {
		if (b) {
			return a->mul(*a, *b);
		} else {
			a->setZero();
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::mul(const BigInt& A, sl_int32 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		if (v) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->mul(*a, v)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::mul(sl_int32 v)
{
	CBigInt* a = m_object.get();
	if (a) {
		if (v) {
			return a->mul(*a, v);
		} else {
			a->setZero();
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::mul(const BigInt& A, sl_uint32 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		if (v) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->mul(*a, v)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::mul(sl_uint32 v)
{
	CBigInt* a = m_object.get();
	if (a) {
		if (v) {
			return a->mul(*a, v);
		} else {
			a->setZero();
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::mul(const BigInt& A, sl_uint64 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		if (v) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->mul(*a, v)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::mul(sl_uint64 v)
{
	CBigInt* a = m_object.get();
	if (a) {
		if (v) {
			return a->mul(*a, v);
		} else {
			a->setZero();
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::div(const BigInt& A, const BigInt& B, BigInt* remainder)
{
	CBigInt* a = A.m_object.get();
	CBigInt* b = B.m_object.get();
	if (b) {
		if (a) {
			CBigInt* q = new CBigInt;
			if (q) {
				if (remainder) {
					CBigInt* r = new CBigInt;
					if (r) {
						if (CBigInt::div(*a, *b, q, r)) {
							*remainder = r;
							return q;
						}
						delete r;
					}
				} else {
					if (CBigInt::div(*a, *b, q, sl_null)) {
						return q;
					}
				}
				delete q;
			}
		}
	}
	if (remainder) {
		*remainder = BigInt::null();
	}
	return BigInt::null();
}

sl_bool BigInt::div(const BigInt& other, BigInt* remainder)
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (b) {
		if (a) {
			if (remainder) {
				CBigInt* r = new CBigInt;
				if (r) {
					if (CBigInt::div(*a, *b, a, r)) {
						*remainder = r;
						return sl_true;
					}
					delete r;
				}
			} else {
				if (CBigInt::div(*a, *b, a, sl_null)) {
					return sl_true;
				}
			}
		} else {
			if (remainder) {
				*remainder = BigInt::null();
			}
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::div(const BigInt& A, sl_int32 v, sl_uint32* remainder)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* q = new CBigInt;
			if (q) {
				if (CBigInt::div(*a, v, q, remainder)) {
					return q;
				}
				delete q;
			}
		}
	}
	if (remainder) {
		*remainder = 0;
	}
	return BigInt::null();
}

sl_bool BigInt::div(sl_int32 v, sl_uint32* remainder)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			if (CBigInt::div(*a, v, a, remainder)) {
				return sl_true;
			}
		} else {
			if (remainder) {
				*remainder = 0;
			}
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::div(const BigInt& A, sl_uint32 v, sl_uint32* remainder)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* q = new CBigInt;
			if (q) {
				if (CBigInt::div(*a, v, q, remainder)) {
					return q;
				}
				delete q;
			}
		}
	}
	if (remainder) {
		*remainder = 0;
	}
	return BigInt::null();
}

sl_bool BigInt::div(sl_uint32 v, sl_uint32* remainder)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			if (CBigInt::div(*a, v, a, remainder)) {
				return sl_true;
			}
		} else {
			if (remainder) {
				*remainder = 0;
			}
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::div(const BigInt& A, sl_int64 v, sl_uint64* remainder)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* q = new CBigInt;
			if (q) {
				if (CBigInt::div(*a, v, q, remainder)) {
					return q;
				}
				delete q;
			}
		}
	}
	if (remainder) {
		*remainder = 0;
	}
	return BigInt::null();
}

sl_bool BigInt::div(sl_int64 v, sl_uint64* remainder)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			if (CBigInt::div(*a, v, a, remainder)) {
				return sl_true;
			}
		} else {
			if (remainder) {
				*remainder = 0;
			}
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::div(const BigInt& A, sl_uint64 v, sl_uint64* remainder)
{
	CBigInt* a = A.m_object.get();
	if (v) {
		if (a) {
			CBigInt* q = new CBigInt;
			if (q) {
				if (CBigInt::div(*a, v, q, remainder)) {
					return q;
				}
				delete q;
			}
		}
	}
	if (remainder) {
		*remainder = 0;
	}
	return BigInt::null();
}

sl_bool BigInt::div(sl_uint64 v, sl_uint64* remainder)
{
	CBigInt* a = m_object.get();
	if (v) {
		if (a) {
			if (CBigInt::div(*a, v, a, remainder)) {
				return sl_true;
			}
		} else {
			if (remainder) {
				*remainder = 0;
			}
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::mod(const BigInt& A, const BigInt& B)
{
	CBigInt* a = A.m_object.get();
	CBigInt* b = B.m_object.get();
	if (b) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (CBigInt::div(*a, *b, sl_null, r)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::mod(const BigInt& other)
{
	CBigInt* a = m_object.get();
	CBigInt* b = other.m_object.get();
	if (b) {
		if (a) {
			return CBigInt::div(*a, *b, sl_null, a);
		} else {
			return sl_true;
		}
	}
	return sl_false;
}

sl_uint32 BigInt::mod(const BigInt& A, sl_int32 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		sl_uint32 r;
		if (CBigInt::div(*a, v, sl_null, &r)) {
			return r;
		}
	}
	return 0;
}

sl_uint32 BigInt::mod(const BigInt& A, sl_uint32 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		sl_uint32 r;
		if (CBigInt::div(*a, v, sl_null, &r)) {
			return r;
		}
	}
	return 0;
}

sl_uint64 BigInt::mod(const BigInt& A, sl_int64 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		sl_uint64 r;
		if (CBigInt::div(*a, v, sl_null, &r)) {
			return r;
		}
	}
	return 0;
}

sl_uint64 BigInt::mod(const BigInt& A, sl_uint64 v)
{
	CBigInt* a = A.m_object.get();
	if (a) {
		sl_uint64 r;
		if (CBigInt::div(*a, v, sl_null, &r)) {
			return r;
		}
	}
	return 0;
}

BigInt BigInt::shiftLeft(const BigInt& A, sl_uint32 n)
{
	CBigInt* a = A.m_object.get();
	if (n) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->shiftLeft(*a, n)) {
					return r;
				}
				delete r;
			}
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::shiftLeft(sl_uint32 n)
{
	CBigInt* a = m_object.get();
	if (n) {
		if (a) {
			return a->shiftLeft(*a, n);
		} else {
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::shiftRight(const BigInt& A, sl_uint32 n)
{
	CBigInt* a = A.m_object.get();
	if (n) {
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->shiftRight(*a, n)) {
					return r;
				}
				delete r;
			}
		}
	} else {
		return a;
	}
	return BigInt::null();
}

sl_bool BigInt::shiftRight(sl_uint32 n)
{
	CBigInt* a = m_object.get();
	if (n) {
		if (a) {
			return a->shiftRight(*a, n);
		} else {
			return sl_true;
		}
	} else {
		return sl_true;
	}
	return sl_false;
}

BigInt BigInt::pow(const BigInt& A, const BigInt& E, const BigInt* pM)
{
	CBigInt* a = A.m_object.get();
	CBigInt* e = E.m_object.get();
	if (!e || e->isZero()) {
		return fromInt32(1);
	}
	if (a) {
		CBigInt* r = new CBigInt;
		if (r) {
			if (pM) {
				if (r->pow(*a, *e, pM->m_object.get())) {
					return r;
				}
			} else {
				if (r->pow(*a, *e, sl_null)) {
					return r;
				}
			}
			delete r;
		}
	}
	return BigInt::null();
}

sl_bool BigInt::pow(const BigInt& E, const BigInt* pM)
{
	CBigInt* a = m_object.get();
	CBigInt* e = E.m_object.get();
	if (!e || e->isZero()) {
		if (a) {
			if (a->setValue(1)) {
				return sl_true;
			}
		} else {
			a = CBigInt::fromInt32(1);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		if (a) {
			if (pM) {
				return a->pow(*a, *e, pM->m_object.get());
			} else {
				return a->pow(*a, *e, sl_null);
			}
		} else {
			return sl_true;
		}
	}
	return sl_false;
}

BigInt BigInt::pow(const BigInt& A, sl_uint32 E, const BigInt* pM)
{
	CBigInt* a = A.m_object.get();
	if (E) {
		if (E == 1) {
			return a;
		}
		if (a) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (pM) {
					if (r->pow(*a, E, pM->m_object.get())) {
						return r;
					}
				} else {
					if (r->pow(*a, E, sl_null)) {
						return r;
					}
				}
				delete r;
			}
		}
	} else {
		return fromInt32(1);
	}
	return BigInt::null();
}

sl_bool BigInt::pow(sl_uint32 E, const BigInt* pM)
{
	CBigInt* a = m_object.get();
	if (E) {
		if (E == 1) {
			sl_true;
		}
		if (a) {
			if (pM) {
				return a->pow(*a, E, pM->m_object.get());
			} else {
				return a->pow(*a, E, sl_null);
			}
		}
	} else {
		if (a) {
			if (a->setValue(1)) {
				return sl_true;
			}
		} else {
			a = CBigInt::fromInt32(1);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	}
	return sl_false;
}

BigInt BigInt::pow_montgomery(const BigInt& A, const BigInt& E, const BigInt& M)
{
	CBigInt* a = A.m_object.get();
	CBigInt* e = E.m_object.get();
	CBigInt* m = M.m_object.get();
	if (!e || e->isZero()) {
		return fromInt32(1);
	} else {
		if (m) {
			if (a) {
				CBigInt* r = new CBigInt;
				if (r) {
					if (r->pow_montgomery(*a, *e, *m)) {
						return r;
					}
					delete r;
				}
			}
		}
	}
	return BigInt::null();
}

sl_bool BigInt::pow_montgomery(const BigInt& E, const BigInt& M)
{
	CBigInt* a = m_object.get();
	CBigInt* e = E.m_object.get();
	CBigInt* m = M.m_object.get();
	if (!e || e->isZero()) {
		if (a) {
			if (a->setValue(1)) {
				return sl_true;
			}
		} else {
			a = CBigInt::fromInt32(1);
			if (a) {
				m_object = a;
				return sl_true;
			}
		}
	} else {
		if (m) {
			if (a) {
				return a->pow_montgomery(*a, *e, *m);
			} else {
				return sl_true;
			}
		}
	}
	return sl_false;
}

BigInt BigInt::gcd(const BigInt& A, const BigInt& B)
{
	CBigInt* a = A.m_object.get();
	CBigInt* b = B.m_object.get();
	if (a) {
		if (b) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->gcd(*a, *b)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

BigInt BigInt::inverseMod(const BigInt& A, const BigInt& M)
{
	CBigInt* a = A.m_object.get();
	CBigInt* m = M.m_object.get();
	if (a) {
		if (m) {
			CBigInt* r = new CBigInt;
			if (r) {
				if (r->inverseMod(*a, *m)) {
					return r;
				}
				delete r;
			}
		}
	}
	return BigInt::null();
}

SLIB_MATH_NAMESPACE_END
