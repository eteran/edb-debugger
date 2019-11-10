
#ifndef GRAPHVIZ_HELPER_H_20190412_
#define GRAPHVIZ_HELPER_H_20190412_

#include <QString>
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

Agnode_t *_agnode(Agraph_t *g, QString name);
int _agset(void *object, QString attr, QString value);
Agsym_t *_agnodeattr(Agraph_t *g, QString name, QString value);
Agsym_t *_agedgeattr(Agraph_t *g, QString name, QString value);
QString _agget(void *object, QString attr, QString alt = QString());
Agraph_t *_agopen(QString name, Agdesc_t kind);

#endif
