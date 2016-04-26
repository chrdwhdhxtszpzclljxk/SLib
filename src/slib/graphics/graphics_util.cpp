#include "../../../inc/slib/graphics/util.h"
#include "../../../inc/slib/core/math.h"

SLIB_GRAPHICS_NAMESPACE_BEGIN

sl_real GraphicsUtil::calculateAlignX(sl_real leftDst, sl_real rightDst, sl_real widthSrc, Alignment align)
{
	Alignment hAlign = align & Alignment::HorizontalMask;
	switch (hAlign) {
		case Alignment::Left:
			break;
		case Alignment::Center:
			return leftDst + (rightDst - leftDst - widthSrc) / 2;
		case Alignment::Right:
			return rightDst - widthSrc;
		default:
			break;
	}
	return leftDst;
}

sl_real GraphicsUtil::calculateAlignY(sl_real topDst, sl_real bottomDst, sl_real heightSrc, Alignment align)
{
	Alignment vAlign = align & Alignment::VerticalMask;
	switch (vAlign) {
		case Alignment::Top:
			break;
		case Alignment::Middle:
			return topDst + (bottomDst - topDst - heightSrc) / 2;
		case Alignment::Bottom:
			return bottomDst - heightSrc;
		default:
			break;
	}
	return topDst;
}

Point GraphicsUtil::calculateAlignPosition(const Rectangle &rcDst, sl_real widthSrc, sl_real heightSrc, Alignment align)
{
	Point ret;
	ret.x = calculateAlignX(rcDst.left, rcDst.right, widthSrc, align);
	ret.y = calculateAlignY(rcDst.top, rcDst.bottom, heightSrc, align);
	return ret;
}

Rectangle GraphicsUtil::transformRectangle(const Rectangle& rectSrc, const Rectangle& rectFrom, const Rectangle& rectTo)
{
	sl_real w1 = rectFrom.getWidth();
	sl_real h1 = rectFrom.getHeight();
	if (Math::isNearZero(w1) || Math::isNearZero(h1)) {
		return Rectangle::zero();
	}
	sl_real w2 = rectTo.getWidth();
	sl_real h2 = rectTo.getHeight();
	if (Math::isNearZero(w2) || Math::isNearZero(h2)) {
		return Rectangle::zero();
	}
	sl_real sw = rectSrc.getWidth();
	sl_real sh = rectSrc.getHeight();
	sl_real w = sw * w2 / w1;
	sl_real h = sh * h2 / h1;
	Rectangle ret;
	ret.left = rectSrc.left + (rectTo.left - rectFrom.left) * sw / w1;
	ret.top = rectSrc.top + (rectTo.top - rectFrom.top) * sh / h1;
	ret.right = ret.left + w;
	ret.bottom = ret.top + h;
	return ret;
}

SLIB_GRAPHICS_NAMESPACE_END
