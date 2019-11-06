
#include "Canvas.h"
#include "ODbgRV_Util.h"

#include <QMouseEvent>
#include <QVBoxLayout>

namespace ODbgRegisterView {

Canvas::Canvas(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	setObjectName("RegViewCanvas");
	const auto canvasLayout = new QVBoxLayout(this);
	canvasLayout->setSpacing(letter_size(parent->font()).height() / 2);
	canvasLayout->setContentsMargins(parent->contentsMargins());
	canvasLayout->setAlignment(Qt::AlignTop);
	setLayout(canvasLayout);
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);
}

void Canvas::mousePressEvent(QMouseEvent *event) {
	event->ignore();
}

}
