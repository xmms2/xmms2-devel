#include <qlistview.h>

#include "xmmslistview.h"

XMMSListViewItem::XMMSListViewItem (XMMSListView *parent, unsigned int id) :
		  QListViewItem (parent)
{
	m_id = id;
}

void
XMMSListViewItem::setArtist (const QString &artist)
{
	m_artist = artist.copy ();
}

void
XMMSListViewItem::setAlbum (const QString &album)
{
	m_album = album.copy ();
}

void
XMMSListViewItem::setTitle (const QString &title)
{
	m_title = title.copy ();
}

void
XMMSListViewItem::setDuration (int duration)
{
	m_duration = duration;
}

void
XMMSListViewItem::setURL (const QString &url)
{
	m_url = url.copy ();
}

QString
XMMSListViewItem::text (int pos) const
{

	switch (pos) {
		case 0:
			return QString::number (m_id);
		case 1:
			if (m_artist)
				return m_artist;

			return m_url;
		case 2:
			return m_album ? m_album : "";
		case 3:
			return m_title ? m_title : "";
		case 4:
			return QString::number (m_duration);
	}
}

XMMSListView::XMMSListView (QWidget *parent, const char *name) :
	      QListView (parent, name)
{

	addColumn ("ID", 50);
	addColumn ("Artist", 200);
	addColumn ("Album", 200);
	addColumn ("Titel", 200);
	addColumn ("Duration", 50);

}


