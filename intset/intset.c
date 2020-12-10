#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "intset.h"

#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

static uint8_t _intsetValueEncoding(int64_t v) {
	if (v < INT32_MIN || v > INT32_MAX)
		return INTSET_ENC_INT64;
	else if (v < INT16_MIN || v > INT16_MAX)
		return INTSET_ENC_INT32;
	else
		return INTSET_ENC_INT16;
}

static int64_t _intsetGetEncoded(intset *is, int pos, uint8_t enc) {
	int64_t v64;
	int32_t v32;
	int16_t v16;

	if (enc == INTSET_ENC_INT64) {
		memcpy(&v64, ((int64_t *)is->contents)+pos, sizeof(v64));
		return v64;
	} else if (enc == INTSET_ENC_INT32) {
		memcpy(&v32, ((int32_t *)is->contents)+pos, sizeof(v32));
		return v32;
	} else {
		memcpy(&v16, ((int16_t *)is->contents)+pos, sizeof(v16));
		return v16;
	}
}

static int64_t _intsetGet(intset *is, int pos) {
	return _intsetGetEncoded(is, pos, is->encoding);
}

static void _intsetSet(intset *is, int pos, int64_t value) {
	if (is->encoding == INTSET_ENC_INT64) {
		((int64_t *)is->contents)[pos] = value;
	} else if (is->encoding == INTSET_ENC_INT32) {
		((int32_t *)is->contents)[pos] = value;
	} else {
		((int16_t *)is->contents)[pos] = value;
	}
}

static intset *intsetResize(intset *is, uint32_t len) {
	return realloc(is, len * is->encoding);
}

/* 搜索 value 在 intset 中的位置。 */
static uint8_t intsetSearch(intset *is, uint64_t value, uint32_t *pos) {
	int min = 0, max = is->length - 1, mid = -1;
	int64_t cur = -1;

	/* intset 为空直接返回 0 */
	if (is->length == 0) {
		if (pos) *pos = 0;
		return 0;
	} else {
		/* value 大于 intset 中最大的值时，pos 为 inset 的长度，
		 * value 小于 intset 中最小的值时，post 为 0，
		 * 不管是大于还是小于，都需要对 intset 进行扩容。 */
		if (value > _intsetGet(is, max)) {
			if (pos) *pos = is->length;
			return 0;
		} else if (value < _intsetGet(is, 0)) {
			*pos = 0;
			return 0;
		}
	}

	/* 二分查找法确定 value 是否在 intset 中。 */
	while (max >= min) {
		/* 右移一位表示除以二。 */
		mid = ((unsigned int)min + (unsigned int)max) >> 1;
		/* 获取处于 mid 的值。 */
		cur = _intsetGet(is, mid);
		/* value > cur 时，表示 value 可能在右边值更大的区域，
		 * value < cur 时，表示 value 可能在左边值更小的区域，
		 * 其他情况说明找到了 value 所在的位置。 */
		if (value > cur) {
			min = mid + 1;
		} else if (value < cur) {
			max = mid - 1;
		} else {
			break;
		}
	}

	if (value == cur) {
		if (pos) *pos = mid;
		return 1;
	} else {
		if (pos) *pos = min;
		return 0;
	}
}

static intset *intsetUpgradeAndAdd(intset *is, int64_t value) {
	uint8_t curenc = is->encoding;
	uint8_t newenc = _intsetValueEncoding(value);
	int length = is->length;
	int prepend = value < 0 ? 1 : 0;

	/* 先设置新的编码和扩容。 */
	is->encoding = newenc;
	is = intsetResize(is, is->length + 1);

	/* 从后向前升级，这样不会覆盖旧值。
	 * 注意，prepend 变量用于确保 intset 开头或结尾有一个空位，用来存储新值 */
	while (length--)
		_intsetSet(is, length + prepend, _intsetGetEncoded(is, length, curenc));

	/* 在 intset 开头或者末尾处设置新值。 */
	if (prepend)
		_intsetSet(is, 0, value);
	else
		_intsetSet(is, is->length, value);

	is->length = is->length + 1;
	return is;
}

static void intsetMoveTail(intset *is, uint32_t from, uint32_t to) {
	void *src, *dst;
	uint32_t bytes = is->length - from;
	uint32_t encoding = is->encoding;

	if (encoding == INTSET_ENC_INT64) {
		src = (int64_t *)is->contents + from;
		dst = (int64_t *)is->contents + to;
		bytes *= sizeof(int64_t);
	} else if (encoding == INTSET_ENC_INT32) {
		src = (int32_t *)is->contents + from;
		dst = (int32_t *)is->contents + to;
		bytes *= sizeof(int32_t);
	} else {
		src = (int16_t *)is->contents + from;
		dst = (int16_t *)is->contents + to;
		bytes *= sizeof(int16_t);
	}
	memmove(dst, src, bytes);
}

intset *intsetNew(void) {
	intset *is = malloc(sizeof(intset));
	is->encoding = INTSET_ENC_INT16;
	is->length = 0;

	return is;
}

intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
	uint8_t valuec = _intsetValueEncoding(value);
	uint32_t pos;
	if (success) *success = 1;

	if (valuec > is->encoding) {
		return intsetUpgradeAndAdd(is, value);
	} else {
		if (intsetSearch(is, value, &pos)) {
			if (success) *success = 0;
			return is;
		}

		is = intsetResize(is, is->length + 1);
		if (pos < is->length) intsetMoveTail(is, pos, pos + 1);
	}

	_intsetSet(is, pos, value);
	is->length = is->length + 1;
	return is;
}

intset *intsetRemove(intset *is, int64_t value, uint8_t *success) {
	uint8_t valenc = _intsetValueEncoding(value);
	uint32_t pos;

	if (valenc <= is->encoding && intsetSearch(is, value, &pos)) {
		uint32_t len = is->length;

		if (success) *success = 1;

		if (pos < (len - 1)) intsetMoveTail(is, pos + 1, pos);
		is = intsetResize(is, len - 1);
		is->length = len - 1;
	}
	return is;
}

uint8_t intsetFind(intset *is, int64_t value) {
	uint8_t valenc = _intsetValueEncoding(value);
	return valenc <= is->encoding && intsetSearch(is, value, NULL);
}

int64_t intsetRandom(intset *is) {
	return _intsetGet(is, rand() % is->length);
}

uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value) {
	if (pos < is->length) {
		*value = _intsetGet(is, pos);
		return 1;
	}
	return 0;
}

uint32_t intsetLen(const intset *is) {
	return is->length;
}

size_t intsetBlobLen(intset *is) {
	return sizeof(intset) + is->length * is->encoding;
}

int main() {
	srand(time(NULL));

	intset *is = intsetNew();

	intsetAdd(is, 1, NULL);
	intsetAdd(is, 3, NULL);
	intsetAdd(is, 5, NULL);
	intsetAdd(is, 7, NULL);

	printf("%ld\n", _intsetGet(is, 2));
	printf("%ld\n", intsetRandom(is));
	printf("%ld\n", intsetBlobLen(is));

	return 1;
}
