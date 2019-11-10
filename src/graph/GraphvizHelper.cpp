
#include "GraphvizHelper.h"

Agnode_t *_agnode(Agraph_t *g, QString name) {
	return agnode(g, name.toLocal8Bit().data(), true);
}

/// Directly use agsafeset which always works, contrarily to agset
int _agset(void *object, QString attr, QString value) {
	return agsafeset(
		object,
		attr.toLocal8Bit().data(),
		value.toLocal8Bit().data(),
		value.toLocal8Bit().data());
}

Agraph_t *_agopen(QString name, Agdesc_t kind) {
	return agopen(name.toLocal8Bit().data(), kind, nullptr);
}

/// Add an alternative value parameter to the method for getting an object's attribute
QString _agget(void *object, QString attr, QString alt) {
	QString str = agget(object, attr.toLocal8Bit().data());

	// TODO(eteran): use isNull()?
	if (str == QString())
		return alt;
	else
		return str;
}

Agsym_t *_agnodeattr(Agraph_t *g, QString name, QString value) {
	return agattr(
		g,
		AGNODE,
		name.toLocal8Bit().data(),
		value.toLocal8Bit().data());
}

Agsym_t *_agedgeattr(Agraph_t *g, QString name, QString value) {
	return agattr(
		g,
		AGEDGE,
		name.toLocal8Bit().data(),
		value.toLocal8Bit().data());
}
