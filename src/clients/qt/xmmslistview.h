#ifndef __XMMSLISTVIEW_H__
#define __XMMSLISTVIEW_H__

#include <qlistview.h>
#include <xmmsclient.h>

class XMMSListView;

class XMMSListViewItem : public QListViewItem
{
public:
	XMMSListViewItem (XMMSListView *parent, unsigned int id, QListViewItem *after);
	QString text (int) const;

	void setArtist (const QString &);
	void setAlbum (const QString &);
	void setTitle (const QString &);
	void setURL (const QString &);
	void setDuration (int);
	void setCurrent (bool);
	void paintCell (QPainter *, const QColorGroup &, int, int, int);
	unsigned int Id () { return m_id; }

protected:
	QString m_artist;
	QString m_album;
	QString m_title;
	QString m_url;
	int m_duration;
	unsigned int m_id;
	bool m_current;
};

class XMMSListView : public QListView
{
	Q_OBJECT
public:
	XMMSListView (xmmsc_connection_t *, QWidget *parent, const char *name);

protected slots:
	void onDoubleClick (QListViewItem *, const QPoint &, int);

protected:
	xmmsc_connection_t *m_connection;

};

#endif
