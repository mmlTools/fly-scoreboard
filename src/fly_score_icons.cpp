#include "fly_score_icons.hpp"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QWidget>

namespace {
class FlyIconEngine final : public QIconEngine {
public:
	FlyIconEngine(FlyIcon icon, QWidget *context) : icon_(icon), context_(context) {}
	QIconEngine *clone() const override { return new FlyIconEngine(icon_, context_); }
	void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
	{
		const auto palette = context_ ? context_->palette() : QApplication::palette();
		const auto color = palette.color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active,
						 QPalette::WindowText);
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		const auto size = qMin(rect.width(), rect.height());
		painter->translate(rect.center().x() - size / 2.0, rect.center().y() - size / 2.0);
		painter->scale(size / 24.0, size / 24.0);
		painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter->setBrush(Qt::NoBrush);
		auto line = [painter](qreal x1, qreal y1, qreal x2, qreal y2) {
			painter->drawLine(QPointF(x1, y1), QPointF(x2, y2));
		};
		QPainterPath path;
		switch (icon_) {
		case FlyIcon::Trash:
			line(4, 6, 20, 6);
			line(9, 3, 15, 3);
			line(9, 10, 9, 17);
			line(15, 10, 15, 17);
			path.moveTo(6, 6);
			path.lineTo(7, 21);
			path.lineTo(17, 21);
			path.lineTo(18, 6);
			break;
		case FlyIcon::Keyboard:
			painter->drawRoundedRect(QRectF(2, 5, 20, 14), 2, 2);
			for (int y : {9, 12})
				for (int x : {6, 10, 14, 18})
					line(x, y, x + 0.5, y);
			line(7, 16, 17, 16);
			break;
		case FlyIcon::Information:
			painter->drawEllipse(QRectF(3, 3, 18, 18));
			line(12, 11, 12, 17);
			line(12, 7, 12, 7.2);
			break;
		case FlyIcon::Folder:
		case FlyIcon::FolderAdd:
			path.moveTo(3, 8);
			path.lineTo(3, 4);
			path.lineTo(9, 4);
			path.lineTo(12, 7);
			path.lineTo(21, 7);
			path.lineTo(21, 20);
			path.lineTo(3, 20);
			path.closeSubpath();
			if (icon_ == FlyIcon::FolderAdd) {
				line(9, 13, 15, 13);
				line(12, 10, 12, 16);
			} else {
				line(3, 10, 21, 10);
			}
			break;
		case FlyIcon::Plus:
			line(5, 12, 19, 12);
			line(12, 5, 12, 19);
			break;
		case FlyIcon::Minus:
			line(5, 12, 19, 12);
			break;
		case FlyIcon::Play:
			path.moveTo(8, 4);
			path.lineTo(20, 12);
			path.lineTo(8, 20);
			path.closeSubpath();
			break;
		case FlyIcon::Pause:
			painter->drawRoundedRect(QRectF(6, 4, 3, 16), 0.5, 0.5);
			painter->drawRoundedRect(QRectF(15, 4, 3, 16), 0.5, 0.5);
			break;
		case FlyIcon::Reset:
			painter->drawArc(QRectF(4, 4, 16, 16), 40 * 16, -295 * 16);
			path.moveTo(4, 4);
			path.lineTo(4, 10);
			path.lineTo(10, 10);
			break;
		case FlyIcon::Heart:
			path.moveTo(12, 21);
			path.cubicTo(9, 18, 2, 13, 2, 8);
			path.cubicTo(2, 2, 9, 1, 12, 6);
			path.cubicTo(15, 1, 22, 2, 22, 8);
			path.cubicTo(22, 13, 15, 18, 12, 21);
			break;
		case FlyIcon::Chat:
			path.moveTo(4, 4);
			path.lineTo(20, 4);
			path.lineTo(20, 17);
			path.lineTo(10, 17);
			path.lineTo(4, 21);
			path.closeSubpath();
			line(8, 9, 16, 9);
			line(8, 13, 13, 13);
			break;
		}
		painter->drawPath(path);
		painter->restore();
	}
	QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
	{
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		paint(&painter, QRect(QPoint(), size), mode, state);
		return pixmap;
	}

private:
	FlyIcon icon_;
	QPointer<QWidget> context_;
};
} // namespace

QIcon fly_icon(FlyIcon icon, QWidget *context)
{
	return QIcon(new FlyIconEngine(icon, context));
}
