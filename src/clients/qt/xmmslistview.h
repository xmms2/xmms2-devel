#ifndef __XMMSLISTVIEW_H__
#define __XMMSLISTVIEW_H__

#include <qlistview.h>

class XMMSListView;

class XMMSListViewItem : public QListViewItem
{
public:
	XMMSListViewItem (XMMSListView *parent, unsigned int id);
	QString text (int) const;

	void setArtist (const QString &);
	void setAlbum (const QString &);
	void setTitle (const QString &);
	void setURL (const QString &);
	void setDuration (int);

protected:
	QString m_artist;
	QString m_album;
	QString m_title;
	QString m_url;
	int m_duration;
	unsigned int m_id;
};

class XMMSListView : public QListView
{
	Q_OBJECT
public:
	XMMSListView (QWidget *parent, const char *name);
};

#endif
